/* scrolller --- generate a parallax scrolling VGA display  2012-02-12 */
/* Copyright (c) 2012 John Honniball, Froods Software Development      */

/* Modification:
 * 2011-12-06 JRH Initial coding as PPM file generator
 * 2012-02-12 JRH Changed to use SVGAlib
 * 2012-02-17 JRH Changed to use ALSA 'asound' library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vga.h>
#include <vgagl.h>
#include <alsa/asoundlib.h>

#define MODE13H

#define FRAMERATE (70)      /* 70 f.p.s. VGA video */
#define SAMPLERATE (44100)  /* CD audio sample rate */

#define NFRAMES   (8 * FRAMERATE)   /* Number of frames to generate */

#define MAXPATH  (256)
#define MAXLINE  (256)
#define MAXSAMPLES (SAMPLERATE / FRAMERATE)

#ifdef MODE13H
#define MAXX     (320)
#define MAXY     (200)
#define MAXX_BG  (640)
#else
#define MAXX     (640)
#define MAXY     (480)
#define MAXX_BG  (1280)
#endif

/* Waveform samples for phase accumulators */
#define NSAMPLES (4096)
#define MAXVOICES (4)
#define MAXTONES  (4)

/* Voices for waveform synthesis */
#define VMUTE     (-1)
#define VSINE     (0)
#define VSQUARE   (1)
#define VTRIANGLE (2)
#define VSAWTOOTH (3)

#define MAXENVELOPES (6)
#define NENVSTEPS    (256)

#define ENVELOPE_LINEAR  (0)   /* Linear decay to zero */
#define ENVELOPE_EXP     (1)   /* Exponential decay, similar to a bell */
#define ENVELOPE_GATE    (2)   /* Simple on/off gating */
#define ENVELOPE_ADSR    (3)   /* Attack, decay, sustain, release */
#define ENVELOPE_SUSTAIN (4)   /* Short exponential attack and decay */
#define ENVELOPE_TREMOLO (5)   /* Like ADSR but with modulated sustain amplitude */

#define TWOPI (2.0 * M_PI)

/* Predefined colours */
#define WHITE   (0xff)
#define RED     (0xe0)
#define GREEN   (0x1c)
#define BLUE    (0x03)
#define MAGENTA (RED | BLUE)
#define BLACK   (0x00)

/* The VGA contexts */
GraphicsContext *Phys = NULL;
GraphicsContext *Virt = NULL;

/* The frame buffer */
unsigned char Frame[MAXY][MAXX];
unsigned char Bgimg[MAXY][MAXX_BG];

#define MAXSPRITES (2)

static unsigned char Player[8][8] = {
   {     0,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,      0},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,   BLUE,   BLUE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,   BLUE,   BLUE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   { WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE},
   {     0,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,  WHITE,      0}
};
static unsigned char Missile[6][7] = {
   {     0,  WHITE,  WHITE,  WHITE,  RED,  RED,    0},
   { WHITE,    RED,    RED,   BLUE,  RED,  RED,  RED},
   { WHITE,    RED,   BLUE,   BLUE, BLUE,  RED,  RED},
   { WHITE,    RED,   BLUE,   BLUE, BLUE,  RED,    1},
   { WHITE,    RED,    RED,   BLUE,  RED,  RED,    1},
   {     0,      1,      1,      1,    1,    1,    0}
};

static unsigned char Ship0[11][29];

/* The software sprites */
struct Sprite {
   int x0, y0;          /* Co-ords of top LH corner */
   int w, h;            /* Width and Height */       
   int visible;         /* Visibility flag */
   int transparent;     /* Colour for transparent pixels */
   unsigned char *tile; /* Pixels of the sprite (h * w) */
};

struct Sprite SpriteTab[MAXSPRITES];

/* Look-up tables for audio synthesis */
short int Wsample[MAXVOICES][NSAMPLES];
short int Wenvelope[MAXENVELOPES][NENVSTEPS];

#define LSAMPLES (25552 / 2)
short int Wlaser[LSAMPLES];

unsigned char Font[256][16] = {
#include "font2.h"
};

/* Tone generation by phase accumulator */
struct ToneRec {
   unsigned int PhaseAcc;
   unsigned int PhaseDelta;
   int PhaseDeltaDelta;
   char Voice;
   char Envelope;
   unsigned short int Ampl;
   unsigned short int Ampr;
   unsigned short int VolumeAcc;
   unsigned short int VolumeDelta;
   unsigned short int EnvAmp;
};

struct ToneRec ToneGen[MAXTONES];

/* A single sample with left and right channels */
struct AudioSample {
   short int r;
   short int l;
};

/* The synthesised or sampled audio signal */
struct AudioSample Audio[MAXSAMPLES];
snd_pcm_t *Pcm = NULL;

int load_assets (void);
int load_background (void);
int load_sprites (void);
int load_sounds (void);
int generate_wavetables (void);
int open_audio (void);
int open_video (void);
void game_logic (int frame);
int clear_bg (int frame);
int draw_sprites (int frame);
int draw_overlay (int frame);
int render_audio (int frame);
int writeframe (int frame);
int writeaudio (int frame);
void close_audio (void);
void close_video (void);


int main (int argc, char *argv[])
{
   int frame;
   
   if (load_assets () != 0)
      exit (1);
   
   open_audio ();
   open_video ();
   
   for (frame = 0; frame < NFRAMES; frame++) {
      game_logic (frame);
      
      clear_bg (frame);
      
      draw_sprites (frame);
      
      draw_overlay (frame);
      
      render_audio (frame);
      
      writeaudio (frame);
      writeframe (frame);
   }
   
   close_video ();
   close_audio ();
   
   exit (0);
}


/* load_assets --- read in any game assets that are stored in files */

int load_assets (void)
{
   if (load_background () != 0)
      return (1);
   
   if (load_sounds () != 0)
      return (1);
      
   if (load_sprites () != 0)
      return (1);
      
   generate_wavetables ();
   
   return (0);
}


/* load_background --- load the scrolling background image from file */

int load_background (void)
{
   FILE *fp;
   char lin[MAXLINE];
   char fname[MAXPATH];
   int x, y;
   int pixel;
   int r, g, b;
   
   sprintf (fname, "bg%d.ppm", MAXY);
   
   if ((fp = fopen (fname, "r")) == NULL) {
      perror (fname);
      return (1);
   }
   
   fgets (lin, MAXLINE, fp);
   fgets (lin, MAXLINE, fp);
   fscanf (fp, "%d %d", &x, &y);
   fscanf (fp, "%d", &pixel);
   
   if ((x != MAXX_BG) || (y != MAXY) || (pixel != 255)) {
      fprintf (stderr, "%s: wrong image size or pixel depth\n", fname);
      fclose (fp);
      return (1);
   }
   
   for (y = 0; y < MAXY; y++) {
      for (x = 0; x < MAXX_BG; x++) {
         fscanf (fp, "%d %d %d", &r, &g, &b);
         r /= 32;
         g /= 32;
         b /= 64;
         Bgimg[y][x] = (r << 5) | (g << 2) | b;
      }
   }
   
   fclose (fp);
   
   return (0);
}


/* load_sprites --- read sprite images from files */

int load_sprites (void)
{
   FILE *fp;
   char lin[MAXLINE];
   char fname[MAXPATH];
   int x, y;
   int pixel;
   int r, g, b;
   
   sprintf (fname, "ship%d.ppm", 0);
   
   if ((fp = fopen (fname, "r")) == NULL) {
      perror (fname);
      return (1);
   }
   
   fgets (lin, MAXLINE, fp);
   fgets (lin, MAXLINE, fp);
   fscanf (fp, "%d %d", &x, &y);
   fscanf (fp, "%d", &pixel);
   
   if ((x != 29) || (y != 11) || (pixel != 255)) {
      fprintf (stderr, "%s: wrong image size or pixel depth\n", fname);
      fclose (fp);
      return (1);
   }
   
   for (y = 0; y < 11; y++) {
      for (x = 0; x < 29; x++) {
         fscanf (fp, "%d %d %d", &r, &g, &b);
         r /= 32;
         g /= 32;
         b /= 64;
         Ship0[y][x] = (r << 5) | (g << 2) | b;
      }
   }
   
   fclose (fp);
   
   return (0);
}


/* load_sounds --- read in audio samples */

int load_sounds (void)
{
   FILE *laser;
   int i;
   short int sample;
   
   if ((laser = fopen ("laser-02.raw", "r")) == NULL) {
      perror ("laser-02.raw");
      return (1);
   }
   
   for (i = 0; i < LSAMPLES; i++) {
      fread (&sample, sizeof (sample), 1, laser);
      Wlaser[i] = sample;
   }
   
   fclose (laser);
   
   return (0);
}


/* generate_wavetables --- generate sine, square, triangle waves */

int generate_wavetables (void)
{
   int i;
   double theta;
   double x;
   short int b;
   
   /* Generate lookup tables for audio waveforms */
   for (i = 0; i < NSAMPLES; i++) {
      /* Sine */
      theta = (TWOPI / NSAMPLES) * (double)i;
      b = (sin (theta) * 2047.0) + 0.5;
      Wsample[0][i] = b;

      /* Square */
      if (i < (NSAMPLES / 2))
         b = -2047;
      else
         b = 2047;

      Wsample[1][i] = b;
      
      /* Triangle */
      if (i == (NSAMPLES / 2)) {
         Wsample[2][i] = 2047;
      }
      else if (i < (NSAMPLES / 2)) {
         Wsample[2][i] = ((i * (2 * 4096)) / NSAMPLES) - 2047;
      }
      else {
         Wsample[2][i] = (((NSAMPLES - i) * (2 * 4096)) / NSAMPLES) - 2047; 
      }
      
      /* Sawtooth */
      Wsample[3][i] = ((i * 4096) / NSAMPLES) - 2047;

/*    sample[4][i] = (((double)rand () * 256.0) / (double)RAND_MAX) - 127; */
   }
   
   /* Generate lookup tables for audio envelopes */
   for (i = 0; i < NENVSTEPS; i++) {
      /* Linear */
      Wenvelope[ENVELOPE_LINEAR][i] = 255 - i;

      /* Exponential decay */
      x = (256 - i) / 32.0;
      b = exp2 (x) - 1.0;
      Wenvelope[ENVELOPE_EXP][i] = b;

      /* On/Off gate */
      if (i < (NSAMPLES - 1))
         Wenvelope[ENVELOPE_GATE][i] = 255;
      else
         Wenvelope[ENVELOPE_GATE][i] = 0;

      /* ADSR */
      if (i < 32) {
         x = (256 - i) / 32.0;
         b = exp2 (x) - 1.0;
      }
      else if (i < 192)
         b = 127;
      else {
         x = (256 - i) / (64.0 / 7.0);
         b = exp2 (x) - 1.0;
      }
      Wenvelope[ENVELOPE_ADSR][i] = b;

      /* Slow attack, sustain, decay */
      if (i < 32) {
         x = (32 - i) / 4.0;
         b = 256.0 - exp2 (x);
      }
      else if (i < 192)
         b = 255;
      else {
         x = (256 - i) / (64.0 / 8.0);
         b = exp2 (x) - 1.0;
      }
      Wenvelope[ENVELOPE_SUSTAIN][i] = b;

      /* Tremolo */
      if (i < 32) {
         x = (256 - i) / 32.0;
         b = exp2 (x) - 1.0;
      }
      else if (i < 192) {
         x = (i - 32) * 5.0 * TWOPI / 160.0;
         b = 127 + (64.0 * sin (x));
      }
      else {
         x = (256 - i) / (64.0 / 7.0);
         b = exp2 (x) - 1.0;
      }
      Wenvelope[ENVELOPE_TREMOLO][i] = b;
   }
   
   /* Initialise audio tone generators */
   for (i = 0; i < MAXTONES; i++) {
      ToneGen[i].Voice = VMUTE;
      ToneGen[i].Envelope = 0;
      ToneGen[i].PhaseAcc = 0;
      ToneGen[i].PhaseDelta = 0;
      ToneGen[i].PhaseDeltaDelta = 0;
      ToneGen[i].VolumeAcc = 0;
      ToneGen[i].VolumeDelta = 0;
      ToneGen[i].EnvAmp = 0;
      ToneGen[i].Ampl = 0;
      ToneGen[i].Ampr = 0;
   }

   return (0);
}


/* open_audio --- open the ALSA PCM channel for writing */

int open_audio (void)
{
   int err;
   char *device = "default";

   if ((err = snd_pcm_open (&Pcm, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
      fprintf (stderr, "Audio setup error: %s\n", snd_strerror (err));
      exit (EXIT_FAILURE);
   }
   
   if ((err = snd_pcm_set_params (Pcm, SND_PCM_FORMAT_S16,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  2, SAMPLERATE, 1, 40000)) < 0) {
      fprintf (stderr, "Audio params error: %s\n", snd_strerror (err));
      exit (EXIT_FAILURE);
   }
   
   return (1);
}


/* open_video --- set up the video mode and buffer(s) */

int open_video (void)
{
   int c;
   int r, g, b;
   
   vga_init ();
   vga_setmode (G320x200x256);   /* Switch into VGA mode 13h */
   gl_setcontextvga (G320x200x256);
   
   Phys = gl_allocatecontext ();
   Virt = gl_allocatecontext ();
   
   if ((Phys == NULL) || (Virt == NULL)) {
      fprintf (stderr, "can't allocate VGA graphics context(s)\n");
      exit (1);
   }
   
   gl_getcontext (Phys);
   
   gl_setcontextvgavirtual (G320x200x256);
   gl_getcontext (Virt);
   
   gl_setcontext (Virt);
   
   for (c = 0; c < 256; c++) {
      r = ((c & 0xe0) >> 5) * 36;
      g = ((c & 0x1c) >> 2) * 36;
      b = (c & 0x03) * 85;
      gl_setpalettecolor (c, r, g, b);
   }
   
   sleep (1);
   
   return (1);
}


/* game_logic --- execute all game logic for this frame */

void game_logic (int frame)
{
   struct Sprite *player = &SpriteTab[0];
   struct Sprite *missile = &SpriteTab[1];

   /* Initialise */
   if (frame == 0) {
      player->x0 = 16;
      player->y0 = frame + 10;
      player->h = 11;
      player->w = 29;
      player->visible = 1;
      player->transparent = MAGENTA;
      player->tile = (unsigned char *)Ship0;

      missile->x0 = 0;
      missile->y0 = 0;
      missile->w = 7;
      missile->h = 6;
      missile->visible = 0;
      missile->transparent = BLACK;
      missile->tile = (unsigned char *)Missile;
   }

   /* Move player sprite */
   player->x0 = 16;
   player->y0 = frame + 10;
   
   /* Clip player to bottom edge of screen */
   if ((player->y0 + player->h) > MAXY)
      player->y0 = MAXY - player->h;

   /* After one second, fire! */
   if (frame == FRAMERATE) {
      missile->x0 = player->x0 + 28;
      missile->y0 = player->y0 + 3;
      missile->visible = 1;

      /* Play note A */
      ToneGen[0].Voice = VSINE;
      ToneGen[0].Envelope = ENVELOPE_TREMOLO;
      ToneGen[0].PhaseAcc = 0;
      ToneGen[0].PhaseDelta = (int)(((4294967295.0 * 440.0) / 44100.0) + 0.5);
      ToneGen[0].PhaseDeltaDelta = -200;
      ToneGen[0].VolumeAcc = 0;
      ToneGen[0].VolumeDelta = 300;
      ToneGen[0].EnvAmp = Wenvelope[ENVELOPE_TREMOLO][0];
      ToneGen[0].Ampl = 2;
      ToneGen[0].Ampr = 8;
   }

   /* Missile is moving */
   if (frame > FRAMERATE) {
      missile->x0 += 2;
      missile->y0 += 1;
   }
   
   /* Clip missile */
   if ((missile->x0 + missile->w) > MAXX)
      missile->x0 = MAXX - missile->w;

   if ((missile->y0 + missile->h) > MAXY)
      missile->y0 = MAXY - missile->h;

   /* After three seconds, play note C */
   if (frame == (FRAMERATE * 3)) {
      ToneGen[0].Voice = VSINE;
      ToneGen[0].Envelope = ENVELOPE_EXP;
      ToneGen[0].PhaseAcc = 0;
      ToneGen[0].PhaseDelta = (int)(((4294967295.0 * 261.0) / 44100.0) + 0.5);
      ToneGen[0].PhaseDeltaDelta = 0;
      ToneGen[0].VolumeAcc = 0;
      ToneGen[0].VolumeDelta = 500;
      ToneGen[0].EnvAmp = Wenvelope[ENVELOPE_EXP][0];
      ToneGen[0].Ampl = 2;
      ToneGen[0].Ampr = 8;
   }                     
}


/* clear_bg --- clear frame by drawing parallax-scrolling background */

int clear_bg (int frame)
{
   int x, y;
   int bx, by;
   
   for (y = 0; y < (MAXY / 4); y++) {
      for (x = 0; x < MAXX; x++) {
         bx = (x + frame) % MAXX_BG;
         by = y;
         Frame[by][x] = Bgimg[by][bx];
         
         bx = (x + (frame * 2)) % MAXX_BG;
         by = y + (MAXY / 4);
         Frame[by][x] = Bgimg[by][bx];

         bx = (x + (frame * 3)) % MAXX_BG;
         by = y + ((MAXY / 4) * 2);
         Frame[by][x] = Bgimg[by][bx];

         bx = (x + (frame * 4)) % MAXX_BG;
         by = y + ((MAXY / 4) * 3);
         Frame[by][x] = Bgimg[by][bx];
      }
   }
}


/* draw_sprites --- draw sprites by overwriting background */

int draw_sprites (int frame)
{
   int x, y;
   int s;
   int sx, sy;
   struct Sprite *sp;
   unsigned char *tp;
   
   /* Loop through sprites, drawing the visible ones */
   /* TODO: loop through in Z-order, from the rear */
   for (s = 0; s < MAXSPRITES; s++) {
      if (SpriteTab[s].visible) {
         sp = &SpriteTab[s];
         tp = sp->tile;
        
         for (y = sp->y0, sy = 0; sy < sp->h; y++, sy++) {
            for (x = sp->x0, sx = 0; sx < sp->w; x++, sx++) {
               if (*tp != sp->transparent) {
                  Frame[y][x] = *tp;
               }
               tp++;
            }
         }
      }
   }
}


/* draw_overlay --- overlay score on top of everything else */

int draw_overlay (int frame)
{
   int x, y;
   int x0, y0;
   int row, col;
   int i;
   char score[32];
   int len;
   unsigned char bitmask;
   
   sprintf (score, "%06d", frame);
   len = strlen (score);
   
   for (i = 0; i < len; i++) {
      x0 = (i * 8) + 8;
      y0 = MAXY - 18;
      
      for (y = y0, row = 0; row < 16; y++, row++) {
         for (x = x0, bitmask = 1, col = 0; col < 8; x++, bitmask <<= 1, col++) {
            if (Font[score[i]][row] & bitmask) {
               Frame[y][x] = WHITE;
            }
            else {
/*             Frame[y][x] = BLACK; */
            }
         }
      }
   }
}


/* render_audio --- generate audio samples for one frame of video */

int render_audio (int frame)
{
   int i;
   int t;
   struct ToneRec *tg;
   int ph;
   int en;
   static int l;
   static unsigned char volumeCounter = 0;
   
   for (i = 0; i < MAXSAMPLES; i++) {
      Audio[i].l = 0;
      Audio[i].r = 0;
      
      volumeCounter++;

      for (t = 0; t < MAXTONES; t++) {
         if (ToneGen[t].Voice != VMUTE) {
            tg = &ToneGen[t];
            ph = (tg->PhaseAcc & 0xfff00000) >> 20;
            tg->PhaseAcc += tg->PhaseDelta;
            tg->PhaseDelta += tg->PhaseDeltaDelta;
            Audio[i].l += (Wsample[tg->Voice][ph] * tg->EnvAmp) / 64;
            Audio[i].r += (Wsample[tg->Voice][ph] * tg->EnvAmp) / 16;
            
            if (volumeCounter == 0) {
               if (tg->VolumeAcc < (65535U - tg->VolumeDelta)) {
                  tg->VolumeAcc += tg->VolumeDelta;
                  en = tg->VolumeAcc >> 8;
                  tg->EnvAmp = Wenvelope[tg->Envelope][en];
               }
               else {
                  tg->Voice = VMUTE;
                  tg->EnvAmp = 0;
                  tg->VolumeAcc = 65535U;
               }
            }
         }
      }
   }
   
   if (frame == FRAMERATE) {
      l = 0;
   }
   
   if ((frame >= FRAMERATE) && (l < LSAMPLES)) {
      for (i = 0; (i < MAXSAMPLES) && (l < LSAMPLES); i++, l++) {
         Audio[i].l += Wlaser[l];
      }
   }
}


/* writeframe --- write a single frame of video to a file */

int writeframe (int frame)
{
   int x, y;
   
   for (y = 0; y < MAXY; y++) {
      for (x = 0; x < MAXX; x++) {
         gl_setpixel (x, y, Frame[y][x]);
      }
   }
   
   vga_waitretrace ();
   gl_copyscreen (Phys);
}


/* writeaudio --- write the current audio buffer to PCM device */

int writeaudio (int frame)
{
   int frames;
   
   frames = snd_pcm_writei (Pcm, Audio, sizeof (Audio) / 4);
   
   if (frames < 0)
      frames = snd_pcm_recover (Pcm, frames, 0);
   
   if (frames < 0)
     fprintf (stderr, "PCM write error: %s\n", snd_strerror (frames));
     
   if ((frames > 0) && (frames < sizeof (Audio) / 4))
     fprintf (stderr, "PCM short write: %i, %i\n", sizeof (Audio), frames);
}


/* close_audio --- close the audio WAV file */

void close_audio (void)
{
   snd_pcm_close (Pcm);
}


/* close_video --- return to text mode */

void close_video (void)
{
   sleep (1);
   
   gl_clearscreen (0);
   vga_setmode (TEXT);
   
   sleep (1);
}
