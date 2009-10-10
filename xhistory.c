/*
 * xhistory.c -- Move list window, part of X front end for XBoard
 *
 * Copyright 2000,2009 Free Software Foundation, Inc.
 * ------------------------------------------------------------------------
 *
 * GNU XBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * GNU XBoard is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.  *
 *
 *------------------------------------------------------------------------
 ** See the file ChangeLog for a revision history.  */

#include "config.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>

#if STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else /* not STDC_HEADERS */
extern char *getenv();
# if HAVE_STRING_H
#  include <string.h>
# else /* not HAVE_STRING_H */
#  include <strings.h>
# endif /* not HAVE_STRING_H */
#endif /* not STDC_HEADERS */

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/cursorfont.h>
#include <X11/Xaw/Text.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Viewport.h>

#include "common.h"
#include "frontend.h"
#include "backend.h"
#include "xboard.h"
#include "xhistory.h"
#include "gettext.h"
#include "gtk/gtk.h"

#ifdef ENABLE_NLS
# define  _(s) gettext (s)
# define N_(s) gettext_noop (s)
#else
# define  _(s) (s)
# define N_(s)  s
#endif

#define _LL_ 100

extern Widget formWidget, shellWidget, boardWidget, menuBarWidget;
extern Display *xDisplay;
extern int squareSize;
extern Pixmap xMarkPixmap;
extern char *layoutName;

extern GtkWidget               *GUI_History;
extern GtkListStore            *LIST_MoveHistory;


struct History{
  String *Nr,*white,*black;
  int     aNr;  /* space actually alocated */
  Widget mvn,mvw,mvb,vbox,viewport,sh;
  char Up;
};

struct History *hist=0;
String dots=" ... ";
Position gameHistoryX, gameHistoryY;

void
HistoryPopDown(object, user_data)
     GtkObject *object;
     gpointer user_data;
{
  gtk_widget_hide (GUI_History);
  return;
}

void HistoryMoveProc(Widget w, XtPointer closure, XtPointer call_data)
{
    int to;
    XawListReturnStruct *R = (XawListReturnStruct *) call_data;
    if (w == hist->mvn || w == hist->mvw) {
      to=2*R->list_index-1;
      ToNrEvent(to);
    }
    else if (w == hist->mvb) {
      to=2*R->list_index;
      ToNrEvent(to);
    }
}


void HistorySet(char movelist[][2*MOVE_LEN],int first,int last,int current)
{
  int i,b,m;
  char movewhite[2*MOVE_LEN],moveblack[2*MOVE_LEN],move[2*MOVE_LEN];
  GtkTreeIter iter;

  /* first clear everything, do we need this? */
  gtk_list_store_clear(LIST_MoveHistory);

  /* copy move list into history window */

  /* go through all moves */
  for(i=0;i<last;i++) 
    {
      /* test if there is a move */
      if(movelist[i][0]) 
	{
	  /* only copy everything before a  ' ' */
	  char* p = strchr(movelist[i], ' ');
	  if (p) 
	    {
	      strncpy(move, movelist[i], p-movelist[i]);
	      move[p-movelist[i]] = NULLCHAR;
	    } 
	  else 
	    {
	      strcpy(move,movelist[i]);
	    }
	} 
      else
	strcpy(move,dots);
      
      if((i%2)==0) 
	{
	  /* white move */
	  strcpy(movewhite,move);
	}
      else
	{
	  /* black move */
	  strcpy(moveblack,move);

	  /* save move */
	  gtk_list_store_append (LIST_MoveHistory, &iter);
	  gtk_list_store_set (LIST_MoveHistory, &iter,
			      0, i,
			      1, movewhite,
			      2, moveblack,
			      -1);

	  strcpy(movewhite,"");
	  strcpy(moveblack,"");
	};
    }
  /* check if ther is a white move left */
  if(movewhite[0])
    {
      i++;
      strcpy(moveblack,"");
      /* save move */
      gtk_list_store_append (LIST_MoveHistory, &iter);
      gtk_list_store_set (LIST_MoveHistory, &iter,
			  0, i,
			  1, movewhite,
			  2, moveblack,
			  -1);
    };
  
  return;
  
  
  if(current<0){
    //	XawListUnhighlight(hist->mvw);
    //	XawListUnhighlight(hist->mvb);
  }
  else if((current%2)==0){
    //	XawListHighlight(hist->mvw, current/2+1);
    //	XawListUnhighlight(hist->mvb);
  }
  else{
    //	XawListUnhighlight(hist->mvw);
    //	if(current) XawListHighlight(hist->mvb, current/2+1);
    //	else XawListUnhighlight(hist->mvb);
  }

return;
}

void HistoryCreate()
{
    String trstr=
             "<Key>Up: BackwardProc() \n \
             <Key>Left: BackwardProc() \n \
             <Key>Down: ForwardProc() \n \
             <Key>Right: ForwardProc() \n";

    return;
}

void
HistoryPopUp()
{
  //  if(!hist) HistoryCreate();

  gtk_widget_show (GUI_History);
  
  return;
}


void
HistoryShowProc(object, user_data)
     GtkObject *object;
     gpointer user_data;
{
  if (!hist) 
    {
      HistoryCreate();
      HistoryPopUp();
    } 
  else if (hist->Up) 
    {
      HistoryPopDown(NULL,NULL);
    } 
  else 
    {
      HistoryPopUp();
    }
  ToNrEvent(currentMove);

  return;
}
