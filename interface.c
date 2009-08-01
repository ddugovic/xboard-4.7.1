#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "common.h"
#include "frontend.h"
#include "backend.h"
#include "moves.h"
#include "xboard.h"
#include "childio.h"
#include "xgamelist.h"
#include "xhistory.h"
#include "xedittags.h"
#include "gettext.h"
#include "callback.h"

#ifdef ENABLE_NLS
# define  _(s) gettext (s)
# define N_(s) gettext_noop (s)
#else
# define  _(s) (s)
# define N_(s)  s
#endif



GdkPixbuf *load_pixbuf(char *filename,int size)
{
  GdkPixbuf *image;

  if(size)
    image = gdk_pixbuf_new_from_file_at_size(filename,size,size,NULL);
  else
    image = gdk_pixbuf_new_from_file(filename,NULL);
  
  if(image == NULL)
    {
      fprintf(stderr,_("Error: couldn't load file: %s\n"),filename);
      exit(1);
    }
  return image;
}

void GUI_DisplayTitle(text)
     char *text;
{
    char title[MSG_SIZ];
    extern GtkWidget *GUI_Window;

    if (text == NULL) text = "";

    /* todo
    if (appData.titleInWindow) {
	i = 0;
	XtSetArg(args[i], XtNlabel, text);   i++;
	XtSetValues(titleWidget, args, i);
	} */

    if (*text != NULLCHAR) {
	strcpy(title, text);
    } else if (appData.icsActive) {
	sprintf(title, "%s: %s", programName, appData.icsHost);
    } else if (appData.cmailGameName[0] != NULLCHAR) {
	sprintf(title, "%s: %s", programName, "CMail");
    } else if (appData.noChessProgram) {
	strcpy(title, programName);
    } else {
	sprintf(title, "%s: %s", programName, first.tidy);
    }
    gtk_window_set_title(GTK_WINDOW(GUI_Window),title);
}

void GUI_SetAspectRatio(ratio)
     int ratio;
{
  /* sets the aspect ration of the main window */
    GdkGeometry hints;
    extern GtkWidget *GUI_Window;

    hints.min_aspect = ratio;
    hints.max_aspect = ratio;
    
    gtk_window_set_geometry_hints (GTK_WINDOW (GUI_Window),
				   GTK_WIDGET (GUI_Window),
				   &hints,
				   GDK_HINT_ASPECT);
    return;
}
