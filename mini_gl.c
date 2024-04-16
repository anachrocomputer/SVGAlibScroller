#include <stdlib.h>
#include <vga.h>
#include <vgagl.h>

GraphicsContext *physicalscreen;
GraphicsContext *virtualscreen;
 
int main(void)
{
   int i,
       j,
       b,
       y,
       c;
 
   vga_init ();
   vga_setmode (G320x200x256);
   gl_setcontextvga (G320x200x256);
   physicalscreen = gl_allocatecontext ();
   gl_getcontext (physicalscreen);
   gl_setcontextvgavirtual (G320x200x256);
   virtualscreen = gl_allocatecontext ();
   gl_getcontext (virtualscreen);
 
   gl_setcontext (virtualscreen);

   /* Set up palette */
   for (c = 1; c <= 64; c++) {
      b = 64 - c;
      gl_setpalettecolor (c, 0, 0, b);
      gl_setpalettecolor (c + 64, 0, b, 0);
   }

   c = 1;
   y = 0;
   for (i = 0; i < 64; i++) {
      for (j = 0; j < 3; j++) {
         gl_hline (0, y, 319, c);
         y++;
      }
      c++;
   }
 
   gl_copyscreen (physicalscreen);
 
   vga_getch (); 

   c = 65;
   y = 0;
   for (i = 0; i < 64; i++) {
      for (j = 0; j < 3; j++) {
         gl_hline (0, y, 319, c);
         y++;
      }
      c++;
   }
 
   gl_copyscreen (physicalscreen);
 
   vga_getch (); 

   gl_clearscreen (0);
   vga_setmode (TEXT);

   return (EXIT_SUCCESS);
}
