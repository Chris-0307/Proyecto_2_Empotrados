/* Reproductor V4 con mediciones de latencia para las pruebas. */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define LW_BASE 0xFF200000u
#define LW_SPAN 0x10000u
#define LEDS 0x0000
#define TIME_BCD 0x0020
#define SWITCHES 0x0040
#define ADC 0x4000
#define AUDIO 0x3040
#define AUD_CONTROL 0
#define AUD_FIFOSPACE 4
#define AUD_LEFT 8
#define AUD_RIGHT 12

/* Registros del FIR (offsets desde LW_BASE). */
#define FIR_CONTROL  0x5000
#define FIR_STATUS   0x5004
#define FIR_DATA_IN  0x5008
#define FIR_DATA_OUT 0x500C

static volatile uint8_t *lw;
static volatile sig_atomic_t stop;
static volatile uint8_t *vga;
static int use_adc;
static int volume = 30;
static int filter = -1;
static int low[2];
static int16_t echo[7200][2];
static unsigned echo_pos, echo_len;
static double last_key[4];
static int button_fd = -1;
/* Tiempos de las pruebas (desde que el programa lee el evento). */
static double inicio_boton = 0, inicio_pista = 0;
static int boton_medido = -1;
static const char *nombres_botones[] = {"KEY0", "KEY1", "KEY2", "KEY3"};
static const char *filters[] = {"Bypass", "Pasa-bajos FIR (HW)", "Pasa-altos", "Eco"};
enum { PLAYING, PAUSED, STOPPED };
enum { FINISHED, NEXT, PREVIOUS, RESTART, ERROR };

struct wav {
    unsigned rate, channels, bits, format;
    uint32_t size;
    long data;
    char title[80], artist[80], album[80];
};

static unsigned le16(const unsigned char *b)
{
    return b[0] | (unsigned)b[1] << 8;
}
static uint32_t le32(const unsigned char *b)
{
    return le16(b) | (uint32_t)le16(b+2) << 16;
}
static int read_wav(FILE *f, struct wav *w)
{
    unsigned char b[16];
    int fmt = 0, data = 0;
    memset(w, 0, sizeof(*w));
    if (fread(b, 1, 12, f) != 12 || memcmp(b,"RIFF",4) || memcmp(b+8,"WAVE",4)) return -1;
    if (fseek(f,0,SEEK_END)) return -1;
    long end = ftell(f);
    if (end < 12 || fseek(f,12,SEEK_SET)) return -1;
    while (fread(b,1,8,f) == 8) {
        uint32_t n = le32(b+4);
        long pos = ftell(f);
        if (pos < 0 || (uint64_t)n + (n&1u) > (uint64_t)(end-pos)) return -1;
        long next = pos + n + (n&1u);
        if (!memcmp(b,"fmt ",4)) {
            if (n < 16 || fread(b,1,16,f) != 16) return -1;
            w->format = le16(b); w->channels = le16(b+2);
            w->rate = le32(b+4); w->bits = le16(b+14);
            if (le16(b+12) != w->channels * 2) return -1;
            fmt = 1;
        } else if (!memcmp(b,"data",4) && !data) {
            w->data = pos; w->size = n; data = 1;
        } else if (!memcmp(b,"LIST",4) && n >= 4 && fread(b,1,4,f)==4 && !memcmp(b,"INFO",4)) {
            long list_end = pos+n;
            while (list_end-ftell(f) >= 8 && fread(b,1,8,f)==8) {
                uint32_t len = le32(b+4);
                long p = ftell(f);
                if ((uint64_t)len + (len&1u) > (uint64_t)(list_end-p)) break;
                char *s = NULL;
                if (!memcmp(b,"INAM",4)) s=w->title;
                if (!memcmp(b,"IART",4)) s=w->artist;
                if (!memcmp(b,"IPRD",4)) s=w->album;
                if (s) {
                    size_t count = len < 79 ? len : 79;
                    if (fread(s,1,count,f)!=count) return -1;
                    s[count]=0;
                }
                if (fseek(f,p+len+(len&1u),SEEK_SET)) return -1;
            }
        }
        if (fseek(f,next,SEEK_SET)) return -1;
    }
    if (!fmt || !data || w->format!=1 || w->bits!=16 || (w->channels!=1 && w->channels!=2)) return -1;
    if (w->rate!=8000 && w->rate!=16000 && w->rate!=44100) return -1;
    if (w->size < w->channels*2 || w->size%(w->channels*2)) return -1;
    return fseek(f,w->data,SEEK_SET);
}
static int read_frame(FILE *f, unsigned channels, int *l, int *r)
{
    unsigned char b[4];
    if (fread(b,2,channels,f)!=channels) return -1;
    *l=(int16_t)le16(b);
    *r=channels==2 ? (int16_t)le16(b+2) : *l;
    return 0;
}
static uint32_t rd(unsigned off) { return *(volatile uint32_t *)(lw+off); }
static void wr(unsigned off,uint32_t v) { *(volatile uint32_t *)(lw+off)=v; }
static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec + t.tv_nsec/1000000000.0;
}
static void medir(const char *tipo, const char *nombre, double inicio, double fin)
{
    printf("LATENCIA %s %s: %.3f ms\n",tipo,nombre,(fin-inicio)*1000);
    fflush(stdout);
}

static void on_signal(int sig) { (void)sig; stop=1; }
static void clear_audio(void) { wr(AUDIO+AUD_CONTROL,8); wr(AUDIO+AUD_CONTROL,0); }
static unsigned audio_space(void)
{
    uint32_t s=rd(AUDIO+AUD_FIFOSPACE);
    unsigned a=(s>>16)&255,b=(s>>24)&255;
    return a<b?a:b;
}
static int wait_space(void)
{
    double start=now();
    while (!stop && !audio_space()) {
        if (now()-start>1.0) { fprintf(stderr,"FIFO sin espacio\n"); return -1; }
    }
    return stop?-1:0;
}
static void send_frame(int l,int r)
{
    /* Formato usado en test_hw.c: muestra de 16 bits en la parte alta. */
    wr(AUDIO+AUD_LEFT,(uint32_t)(uint16_t)l<<16);
    wr(AUDIO+AUD_RIGHT,(uint32_t)(uint16_t)r<<16);
}
static void display_time(unsigned seconds)
{
    unsigned mm=(seconds/60)%100, ss=seconds%60;
    wr(TIME_BCD,((mm/10)<<12)|((mm%10)<<8)|((ss/10)<<4)|(ss%10));
}
static int probe(void)
{
    unsigned count=88200;
    clear_audio();
    double start=now();
    while (count && !stop) {
        if (wait_space()) return -1;
        unsigned k=audio_space();
        while (k-- && count) { send_frame(0,0); count--; }
    }
    double elapsed=now()-start;
    if (stop || elapsed<=0) return -1;
    printf("88200 frames en %.3f s; frecuencia aproximada %.0f Hz\n",elapsed,88200.0/elapsed);
    return 0;
}
static void sleep_ms(int ms)
{
    struct timespec t = {ms/1000, (ms%1000)*1000000L};
    nanosleep(&t, NULL);
}

/* El driver atiende la IRQ y guarda la mascara de botones. */
static int start_buttons(void)
{
    button_fd = open("/dev/fpga_buttons", O_RDONLY | O_NONBLOCK);
    if (button_fd < 0) {
        perror("/dev/fpga_buttons");
        fprintf(stderr, "Cargue el driver buttons.ko antes de reproducir.\n");
        return -1;
    }
    printf("Botones: driver de interrupciones activo\n");
    return 0;
}

static void stop_buttons(void)
{
    if (button_fd >= 0) close(button_fd);
    button_fd = -1;
}

/* Leer el evento guardado por el driver sin detener el audio. */
static int buttons(void)
{
    unsigned char e;
    ssize_t n = read(button_fd, &e, 1);
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) return 0;
    if (n != 1) return -1;

    int result = 0;
    double t = now();
    for (int i=0; i<4; i++) {
        if ((e & (1u<<i)) && t-last_key[i]>0.15) {
            result |= 1u<<i;
            last_key[i] = t;
        }
    }
    return result;
}

static int limit(int x)
{
    if (x>32767) return 32767;
    if (x<-32768) return -32768;
    return x;
}

static void reset_filter(unsigned rate)
{
    low[0]=low[1]=0;
    memset(echo,0,sizeof(echo));
    echo_pos=0;
    echo_len=rate*150/1000;
}

/* STATUS: bit 0 listo para escribir, bit 1 resultado disponible. */
static int esperar_fir(uint32_t mascara)
{
    for (int i=0; i<1000000 && !stop; i++) {
        if (rd(FIR_STATUS) & mascara) return 0;
    }
    if (!stop)
        fprintf(stderr,"FIR: timeout, STATUS = 0x%X\n",rd(FIR_STATUS));
    return -1;
}

static int apply_filter(int *l,int *r)
{
    if (filter==1) {
        if (esperar_fir(1)) return -1;
        wr(FIR_DATA_IN,((uint32_t)(uint16_t)*r<<16)|(uint16_t)*l);
        if (esperar_fir(2)) return -1;
        uint32_t dato=rd(FIR_DATA_OUT);
        *l=(int16_t)(dato & 0xFFFF);
        *r=(int16_t)(dato >> 16);
        return 0;
    }

    int x[2]={*l,*r};
    for (int ch=0;ch<2;ch++) {
        if (filter==2) {
            low[ch]+=(x[ch]-low[ch])/12;
            x[ch]=x[ch]-low[ch];
        } else if (filter==3) {
            int old=echo[echo_pos][ch];
            echo[echo_pos][ch]=x[ch];
            x[ch]=(2*x[ch]+old)/3;
        }
    }
    if (filter==3 && ++echo_pos>=echo_len) echo_pos=0;
    *l=limit(x[0]); *r=limit(x[1]);
    return 0;
}

static void vga_line(unsigned row,const char *s)
{
    if (!vga) return;
    int ended=0;
    for (unsigned i=0;i<80;i++) {
        if (!ended && !s[i]) ended=1;
        unsigned char c=ended?' ':s[i];
        vga[row*128+i]=(c>=32 && c<=126)?c:' ';
    }
}

static void show(struct wav *w,const char *name,int track,int count,
                 int state,unsigned seconds)
{
    char line[160];
    const char *states[]={"PLAY", "PAUSA", "STOP"};
    display_time(seconds);
    wr(LEDS,(1u<<state)|((unsigned)filter<<4));
    if (!vga) return;
    vga_line(2,"PROYECTO 2 - REPRODUCTOR DE AUDIO");
    snprintf(line,sizeof(line),"Pista: %d / %d",track,count); vga_line(5,line);
    snprintf(line,sizeof(line),"Titulo: %s",w->title[0]?w->title:name); vga_line(7,line);
    snprintf(line,sizeof(line),"Artista: %s",w->artist[0]?w->artist:"-"); vga_line(8,line);
    snprintf(line,sizeof(line),"Album: %s",w->album[0]?w->album:"-"); vga_line(9,line);
    unsigned duration=w->size/(w->rate*w->channels*2);
    snprintf(line,sizeof(line),"Tiempo: %02u:%02u / %02u:%02u",seconds/60,seconds%60,duration/60,duration%60);
    vga_line(12,line);
    snprintf(line,sizeof(line),"Estado: %s",states[state]); vga_line(14,line);
    snprintf(line,sizeof(line),"Filtro: %s",filters[filter]); vga_line(15,line);
    snprintf(line,sizeof(line),"Volumen: %d%%",volume); vga_line(16,line);
    vga_line(20,"KEY0 Play/Pausa  KEY1 Stop  KEY2 Anterior  KEY3 Siguiente");
}

static int play(FILE *f,struct wav *w,unsigned out_rate,const char *name,
                int track,int count,int initial_state,int *next_state)
{
    unsigned frames=w->size/(w->channels*2), index=0, phase=0;
    uint64_t total=((uint64_t)frames*out_rate+w->rate-1)/w->rate;
    int l,r,nl,nr,state=initial_state;
    if (read_frame(f,w->channels,&l,&r)) return ERROR;
    nl=l; nr=r;
    if (frames>1 && read_frame(f,w->channels,&nl,&nr)) return ERROR;
    clear_audio(); reset_filter(out_rate);
    filter=-1;
    unsigned last=~0u;
    int last_state=-1,last_volume=-1;
    double last_space=now(), last_adc=0, last_buttons=0;
    double inicio_filtro=0;
    uint64_t i=0;
    while (!stop) {
        int e=0;
        double key_time=now();
        if (key_time-last_buttons>=0.005) {
            e=buttons();
            last_buttons=key_time;
        }
        if (e<0) { fprintf(stderr,"Error leyendo los botones\n"); return ERROR; }
        if (e) {
            inicio_boton=key_time;
            boton_medido=(e&8)?3:(e&4)?2:(e&2)?1:0;
        }
        if (e&8) { inicio_pista=state==PLAYING?key_time:0; *next_state=state; return NEXT; }
        if (e&4) { inicio_pista=state==PLAYING?key_time:0; *next_state=state; return PREVIOUS; }
        if (e&2) { inicio_pista=0; *next_state=STOPPED; return RESTART; }
        if (e&1) {
            state=state==PLAYING?PAUSED:PLAYING;
            printf("Estado: %s\n",state==PLAYING?"PLAY":"PAUSA"); fflush(stdout);
        }
        double switch_time=now();
        int selected=rd(SWITCHES)&3;
        int changed=selected!=filter;
        if (changed) {
            /* No contar el filtro inicial de cada pista. */
            inicio_filtro=(filter>=0 && state==PLAYING)?switch_time:0;
            filter=selected; reset_filter(out_rate);
            if (filter==1) {
                wr(FIR_CONTROL,1);
                if (esperar_fir(1)) return ERROR;
            }
            printf("Filtro: %s\n",filters[filter]); fflush(stdout);
        }
        double t=now();
        if (use_adc && t-last_adc>=0.05) {
            /* ADC_IN0, escala de 0 a 30% para las primeras pruebas. */
            volume=(rd(ADC)&4095)*30/4095;
            last_adc=t;
        }
        unsigned seconds=i/out_rate;
        if (last!=seconds || last_state!=state || changed || last_volume!=volume) {
            show(w,name,track,count,state,seconds);
            last=seconds; last_state=state; last_volume=volume;
        }
        if (boton_medido>=0) {
            double fin=now();
            medir("BOTON",nombres_botones[boton_medido],inicio_boton,fin);
            boton_medido=-1;
        }
        if (state!=PLAYING) {
            inicio_filtro=0;
            sleep_ms(10); last_space=now(); continue;
        }
        if (i>=total) {
            /* La FIFO tiene hasta 128 frames: esperar el final del audio. */
            sleep_ms(20); *next_state=PLAYING; return FINISHED;
        }
        unsigned k=audio_space();
        if (!k) {
            if (t-last_space>1.0) { fprintf(stderr,"FIFO sin espacio\n"); return ERROR; }
            continue;
        }
        last_space=t;
        while (k-- && i<total && !stop) {
            int a=l+(nl-l)*(int64_t)phase/out_rate;
            int b=r+(nr-r)*(int64_t)phase/out_rate;
            if (apply_filter(&a,&b)) return ERROR;
            send_frame(a*volume/100,b*volume/100);
            if (inicio_pista>0 || inicio_filtro>0) {
                double fin=now();
                if (inicio_pista>0) {
                    medir("PISTA","primer_frame",inicio_pista,fin);
                    inicio_pista=0;
                }
                if (inicio_filtro>0) {
                    medir("FILTRO",filters[filter],inicio_filtro,fin);
                    inicio_filtro=0;
                }
            }
            i++; phase+=w->rate;
            while (phase>=out_rate) {
                phase-=out_rate;
                if (index+1<frames) {
                    index++; l=nl; r=nr;
                    if (index+1<frames) {
                        if (read_frame(f,w->channels,&nl,&nr)) return ERROR;
                    } else { nl=l; nr=r; }
                }
            }
        }
    }
    return ERROR;
}
int main(int argc,char **argv)
{
    if (argc<2) {
        printf("Uso: %s --probe | --info archivo.wav | 44100 archivo.wav [otro.wav ...]\n",argv[0]);
        return 1;
    }
    int info=!strcmp(argv[1],"--info"), measure=!strcmp(argv[1],"--probe");
    unsigned rate=44100;
    if (!measure && argc<3) return 1;
    if (!info && !measure) {
        rate=atoi(argv[1]);
        if (rate!=44100) { fprintf(stderr,"Frecuencia de salida: 44100 Hz (FIR)\n"); return 1; }
    }
    int fd=-1, result=0;
    if (!info) {
        fd=open("/dev/mem",O_RDWR|O_SYNC);
        if (fd<0) { perror("/dev/mem"); return 1; }
        void *p=mmap(NULL,LW_SPAN,PROT_READ|PROT_WRITE,MAP_SHARED,fd,(off_t)LW_BASE);
        if (p==MAP_FAILED) { perror("mmap"); close(fd); return 1; }
        lw=p;
        signal(SIGINT,on_signal); signal(SIGTERM,on_signal);
        if (!measure) {
            if (start_buttons()) {
                munmap((void *)lw,LW_SPAN);
                close(fd);
                return 1;
            }
            use_adc=getenv("PLAYER_ADC")!=NULL;
            if (use_adc) { wr(ADC+4,1); sleep_ms(50); }
            if (getenv("PLAYER_VGA")) {
                void *p=mmap(NULL,0x40000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,(off_t)0xC8000000u);
                if (p!=MAP_FAILED) {
                    volatile uint16_t *pix=p;
                    for (unsigned i=0;i<0x40000/2;i++) pix[i]=0;
                    munmap(p,0x40000);
                } else perror("VGA pixel");
                p=mmap(NULL,0x2000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,(off_t)0xC9000000u);
                if (p!=MAP_FAILED) {
                    vga=p;
                    for (unsigned i=0;i<0x2000;i++) vga[i]=' ';
                } else perror("VGA caracteres");
            }
        }
    }
    if (measure) result=probe()!=0;
    else {
        int i=2, state=PLAYING, bad=0;
        while (i<argc && !stop) {
            FILE *f=fopen(argv[i],"rb"); struct wav w;
            if (!f || read_wav(f,&w)) {
                fprintf(stderr,"No se pudo leer WAV: %s\n",argv[i]);
                if (f) fclose(f);
                result=1; bad++; i++;
                if (bad>=argc-2) break;
                if (!info && i>=argc) i=2;
                continue;
            }
            bad=0;
            printf("Pista %d/%d: %s\n%u Hz, %u canales, %u bits, %.2f s\n",i-1,argc-2,argv[i],w.rate,w.channels,w.bits,w.size/(double)(w.rate*w.channels*2));
            printf("Titulo: %s | Artista: %s | Album: %s\n",w.title[0]?w.title:argv[i],w.artist[0]?w.artist:"-",w.album[0]?w.album:"-");
            fflush(stdout);
            if (info) { fclose(f); i++; continue; }
            int action=play(f,&w,rate,argv[i],i-1,argc-2,state,&state);
            fclose(f); clear_audio();
            if (action==ERROR) { result=stop?0:1; break; }
            if (action==PREVIOUS) i=i==2?argc-1:i-1;
            if (action==NEXT || action==FINISHED) i=i==argc-1?2:i+1;
        }
    }
    if (!info) {
        stop_buttons();
        clear_audio(); wr(LEDS,0); display_time(0);
        if (vga) munmap((void *)vga,0x2000);
        munmap((void *)lw,LW_SPAN); close(fd);
    }
    return result;
}
