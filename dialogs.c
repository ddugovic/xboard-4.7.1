/*
 * dialogs.c -- platform-independent code for dialogs of XBoard
 *
 * Copyright 2000, 2009, 2010, 2011, 2012 Free Software Foundation, Inc.
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

// [HGM] this file is the counterpart of woptions.c, containing xboard popup menus
// similar to those of WinBoard, to set the most common options interactively.

#include "config.h"

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
#include <stdint.h>

#include "common.h"
#include "backend.h"
#include "xboard.h"
#include "menus.h"
#include "dialogs.h"
#include "gettext.h"

#ifdef ENABLE_NLS
# define  _(s) gettext (s)
# define N_(s) gettext_noop (s)
#else
# define  _(s) (s)
# define N_(s)  s
#endif


int values[MAX_OPTIONS];
ChessProgramState *currentCps;
ButtonCallback *comboCallback;

//----------------------------Generic dialog --------------------------------------------

// cloned from Engine Settings dialog (and later merged with it)

char *marked[NrOfDialogs];
Boolean shellUp[NrOfDialogs];

void
MarkMenu (char *item, int dlgNr)
{
    MarkMenuItem(marked[dlgNr] = item, True);
}

void
AddLine (Option *opt, char *s)
{
    AppendText(opt, s);
    AppendText(opt, "\n");
}

//---------------------------------------------- Update dialog controls ------------------------------------

void
GenericUpdate (Option *opts, int selected)
{
    int i, j;
    char buf[MSG_SIZ];
    float x;
	for(i=0; ; i++) {
	    if(selected >= 0) { if(i < selected) continue; else if(i > selected) break; }
	    switch(opts[i].type) {
		case TextBox:
		case FileName:
		case PathName:
		    SetWidgetText(&opts[i],  *(char**) opts[i].target, -1);
		    break;
		case Spin:
		    sprintf(buf, "%d", *(int*) opts[i].target);
		    SetWidgetText(&opts[i], buf, -1);
		    break;
		case Fractional:
		    sprintf(buf, "%4.2f", *(float*) opts[i].target);
		    SetWidgetText(&opts[i], buf, -1);
		    break;
		case CheckBox:
		    SetWidgetState(&opts[i],  *(Boolean*) opts[i].target);
		    break;
		case ComboBox:
		    for(j=0; opts[i].choice[j]; j++)
			if(*(char**)opts[i].target && !strcmp(*(char**)opts[i].target, opts[i].choice[j])) break;
		    values[i] = opts[i].value = j + (opts[i].choice[j] == NULL);
		    // TODO: actually display this
		    break;
		case EndMark:
		    return;
	    default:
		printf("GenericUpdate: unexpected case in switch.\n");
		case Button:
		case SaveButton:
		case Label:
		case Break:
	      break;
	    }
	}
}

//------------------------------------------- Read out dialog controls ------------------------------------

int
GenericReadout (Option *opts, int selected)
{
    int i, j, res=1;
    char *val;
    char buf[MSG_SIZ], **dest;
    float x;
	for(i=0; ; i++) { // send all options that had to be OK-ed to engine
	    if(selected >= 0) { if(i < selected) continue; else if(i > selected) break; }
	    switch(opts[i].type) {
		case TextBox:
		case FileName:
		case PathName:
		    GetWidgetText(&opts[i], &val);
		    dest = currentCps ? &(opts[i].textValue) : (char**) opts[i].target;
		    if(*dest == NULL || strcmp(*dest, val)) {
			if(currentCps) {
			    snprintf(buf, MSG_SIZ,  "option %s=%s\n", opts[i].name, val);
			    SendToProgram(buf, currentCps);
			} else {
			    if(*dest) free(*dest);
			    *dest = malloc(strlen(val)+1);
			}
			safeStrCpy(*dest, val, MSG_SIZ - (*dest - opts[i].name)); // copy text there
		    }
		    break;
		case Spin:
		case Fractional:
		    GetWidgetText(&opts[i], &val);
		    x = 0.0; // Initialise because sscanf() will fail if non-numeric text is entered
		    sscanf(val, "%f", &x);
		    if(x > opts[i].max) x = opts[i].max;
		    if(x < opts[i].min) x = opts[i].min;
		    if(opts[i].type == Fractional)
			*(float*) opts[i].target = x; // engines never have float options!
		    else if(opts[i].value != x) {
			opts[i].value = x;
			if(currentCps) {
			    snprintf(buf, MSG_SIZ,  "option %s=%.0f\n", opts[i].name, x);
			    SendToProgram(buf, currentCps);
			} else *(int*) opts[i].target = x;
		    }
		    break;
		case CheckBox:
		    j = 0;
		    GetWidgetState(&opts[i], &j);
		    if(opts[i].value != j) {
			opts[i].value = j;
			if(currentCps) {
			    snprintf(buf, MSG_SIZ,  "option %s=%d\n", opts[i].name, j);
			    SendToProgram(buf, currentCps);
			} else *(Boolean*) opts[i].target = j;
		    }
		    break;
		case ComboBox:
		    val = ((char**)opts[i].choice)[values[i]];
		    if(currentCps) {
			if(opts[i].value == values[i]) break; // not changed
			opts[i].value = values[i];
			snprintf(buf, MSG_SIZ,  "option %s=%s\n", opts[i].name,
				((char**)opts[i].textValue)[values[i]]);
			SendToProgram(buf, currentCps);
		    } else if(val && (*(char**) opts[i].target == NULL || strcmp(*(char**) opts[i].target, val))) {
		      if(*(char**) opts[i].target) free(*(char**) opts[i].target);
		      *(char**) opts[i].target = strdup(val);
		    }
		    break;
		case EndMark:
		    if(opts[i].target) // callback for implementing necessary actions on OK (like redraw)
			res = ((OKCallback*) opts[i].target)(i);
		    break;
	    default:
		printf("GenericReadout: unexpected case in switch.\n");
		case Button:
		case SaveButton:
		case Label:
		case Break:
	      break;
	    }
	    if(opts[i].type == EndMark) break;
	}
	return res;
}

//------------------------------------------- Match Options ------------------------------------------------------

char *engineName, *engineChoice, *tfName;
char *engineList[MAXENGINES] = {" "}, *engineMnemonic[MAXENGINES] = {""};

static void AddToTourney P((int n));
static void CloneTourney P((void));
static void ReplaceParticipant P((void));
static void UpgradeParticipant P((void));

static int
MatchOK (int n)
{
    ASSIGN(appData.participants, engineName);
    if(!CreateTourney(tfName) || matchMode) return matchMode || !appData.participants[0];
    PopDown(TransientDlg); // early popdown to prevent FreezeUI called through MatchEvent from causing XtGrab warning
    MatchEvent(2); // start tourney
    return 1;
}

static Option matchOptions[] = {
{ 0,  0,          0, NULL, (void*) &tfName, ".trn", NULL, FileName, N_("Tournament file:") },
{ 0,  0,          0, NULL, (void*) &appData.roundSync, "", NULL, CheckBox, N_("Sync after round    (for concurrent playing of a single") },
{ 0,  0,          0, NULL, (void*) &appData.cycleSync, "", NULL, CheckBox, N_("Sync after cycle      tourney with multiple XBoards)") },
{ 0xD, 150,       0, NULL, (void*) &engineName, "", NULL, TextBox, N_("Tourney participants:") },
{ 0,  COMBO_CALLBACK | NO_GETTEXT,
		  0, NULL, (void*) &engineChoice, (char*) (engineMnemonic+1), (engineMnemonic+1), ComboBox, N_("Select Engine:") },
{ 0,  0,         10, NULL, (void*) &appData.tourneyType, "", NULL, Spin, N_("Tourney type (0 = round-robin, 1 = gauntlet):") },
{ 0,  1, 1000000000, NULL, (void*) &appData.tourneyCycles, "", NULL, Spin, N_("Number of tourney cycles (or Swiss rounds):") },
{ 0,  1, 1000000000, NULL, (void*) &appData.defaultMatchGames, "", NULL, Spin, N_("Default Number of Games in Match (or Pairing):") },
{ 0,  0, 1000000000, NULL, (void*) &appData.matchPause, "", NULL, Spin, N_("Pause between Match Games (msec):") },
{ 0,  0,          0, NULL, (void*) &appData.saveGameFile, ".pgn", NULL, FileName, N_("Save Tourney Games on:") },
{ 0,  0,          0, NULL, (void*) &appData.loadGameFile, ".pgn", NULL, FileName, N_("Game File with Opening Lines:") },
{ 0, -2, 1000000000, NULL, (void*) &appData.loadGameIndex, "", NULL, Spin, N_("Game Number (-1 or -2 = Auto-Increment):") },
{ 0,  0,          0, NULL, (void*) &appData.loadPositionFile, ".fen", NULL, FileName, N_("File with Start Positions:") },
{ 0, -2, 1000000000, NULL, (void*) &appData.loadPositionIndex, "", NULL, Spin, N_("Position Number (-1 or -2 = Auto-Increment):") },
{ 0,  0, 1000000000, NULL, (void*) &appData.rewindIndex, "", NULL, Spin, N_("Rewind Index after this many Games (0 = never):") },
{ 0,  0,          0, NULL, (void*) &appData.defNoBook, "", NULL, CheckBox, N_("Disable own engine books by default") },
{ 0,  0,          0, NULL, (void*) &ReplaceParticipant, NULL, NULL, Button, N_("Replace Engine") },
{ 0,  1,          0, NULL, (void*) &UpgradeParticipant, NULL, NULL, Button, N_("Upgrade Engine") },
{ 0,  1,          0, NULL, (void*) &CloneTourney, NULL, NULL, Button, N_("Clone Tourney") },
{ 0, 1, 0, NULL, (void*) &MatchOK, "", NULL, EndMark , "" }
};

static void
ReplaceParticipant ()
{
    GenericReadout(matchOptions, 3);
    Substitute(strdup(engineName), True);
}

static void
UpgradeParticipant ()
{
    GenericReadout(matchOptions, 3);
    Substitute(strdup(engineName), False);
}

static void
CloneTourney ()
{
    FILE *f;
    char *name;
    GetWidgetText(matchOptions, &name);
    if(name && name[0] && (f = fopen(name, "r")) ) {
	char *saveSaveFile;
	saveSaveFile = appData.saveGameFile; appData.saveGameFile = NULL; // this is a persistent option, protect from change
	ParseArgsFromFile(f);
	engineName = appData.participants; GenericUpdate(matchOptions, -1);
	FREE(appData.saveGameFile); appData.saveGameFile = saveSaveFile;
    } else DisplayError(_("First you must specify an existing tourney file to clone"), 0);
}

static void
AddToTourney (int n)
{
    GenericReadout(matchOptions, 4);  // selected engine
    AddLine(&matchOptions[3], engineChoice);
}

void
MatchOptionsProc ()
{
   NamesToList(firstChessProgramNames, engineList, engineMnemonic, "all");
   comboCallback = &AddToTourney;
   matchOptions[5].min = -(appData.pairingEngine[0] != NULLCHAR); // with pairing engine, allow Swiss
   ASSIGN(tfName, appData.tourneyFile[0] ? appData.tourneyFile : MakeName(appData.defName));
   ASSIGN(engineName, appData.participants);
   GenericPopUp(matchOptions, _("Match Options"), TransientDlg);
}

// ------------------------------------------- General Options --------------------------------------------------

static int oldShow, oldBlind, oldPonder;

static int
GeneralOptionsOK (int n)
{
	int newPonder = appData.ponderNextMove;
	appData.ponderNextMove = oldPonder;
	PonderNextMoveEvent(newPonder);
	if(!appData.highlightLastMove) ClearHighlights(), ClearPremoveHighlights();
	if(oldShow != appData.showCoords || oldBlind != appData.blindfold) DrawPosition(TRUE, NULL);
	return 1;
}

static Option generalOptions[] = {
{ 0,  0, 0, NULL, (void*) &appData.whitePOV, "", NULL, CheckBox, N_("Absolute Analysis Scores") },
{ 0,  0, 0, NULL, (void*) &appData.sweepSelect, "", NULL, CheckBox, N_("Almost Always Queen (Detour Under-Promote)") },
{ 0,  0, 0, NULL, (void*) &appData.animateDragging, "", NULL, CheckBox, N_("Animate Dragging") },
{ 0,  0, 0, NULL, (void*) &appData.animate, "", NULL, CheckBox, N_("Animate Moving") },
{ 0,  0, 0, NULL, (void*) &appData.autoCallFlag, "", NULL, CheckBox, N_("Auto Flag") },
{ 0,  0, 0, NULL, (void*) &appData.autoFlipView, "", NULL, CheckBox, N_("Auto Flip View") },
{ 0,  0, 0, NULL, (void*) &appData.blindfold, "", NULL, CheckBox, N_("Blindfold") },
{ 0,  0, 0, NULL, (void*) &appData.dropMenu, "", NULL, CheckBox, N_("Drop Menu") },
{ 0,  0, 0, NULL, (void*) &appData.hideThinkingFromHuman, "", NULL, CheckBox, N_("Hide Thinking from Human") },
{ 0,  0, 0, NULL, (void*) &appData.highlightLastMove, "", NULL, CheckBox, N_("Highlight Last Move") },
{ 0,  0, 0, NULL, (void*) &appData.highlightMoveWithArrow, "", NULL, CheckBox, N_("Highlight with Arrow") },
{ 0,  0, 0, NULL, (void*) &appData.ringBellAfterMoves, "", NULL, CheckBox, N_("Move Sound") },
{ 0,  0, 0, NULL, (void*) &appData.oneClick, "", NULL, CheckBox, N_("One-Click Moving") },
{ 0,  0, 0, NULL, (void*) &appData.periodicUpdates, "", NULL, CheckBox, N_("Periodic Updates (in Analysis Mode)") },
{ 0,  0, 0, NULL, (void*) &appData.ponderNextMove, "", NULL, CheckBox, N_("Ponder Next Move") },
{ 0,  0, 0, NULL, (void*) &appData.popupExitMessage, "", NULL, CheckBox, N_("Popup Exit Messages") },
{ 0,  0, 0, NULL, (void*) &appData.popupMoveErrors, "", NULL, CheckBox, N_("Popup Move Errors") },
{ 0,  0, 0, NULL, (void*) &appData.showEvalInMoveHistory, "", NULL, CheckBox, N_("Scores in Move List") },
{ 0,  0, 0, NULL, (void*) &appData.showCoords, "", NULL, CheckBox, N_("Show Coordinates") },
{ 0,  0, 0, NULL, (void*) &appData.markers, "", NULL, CheckBox, N_("Show Target Squares") },
{ 0,  0, 0, NULL, (void*) &appData.useStickyWindows, "", NULL, CheckBox, N_("Sticky Windows") },
{ 0,  0, 0, NULL, (void*) &appData.testLegality, "", NULL, CheckBox, N_("Test Legality") },
{ 0, 0, 10, NULL, (void*) &appData.flashCount, "", NULL, Spin, N_("Flash Moves (0 = no flashing):") },
{ 0, 1, 10, NULL, (void*) &appData.flashRate, "", NULL, Spin, N_("Flash Rate (high = fast):") },
{ 0, 5, 100,NULL, (void*) &appData.animSpeed, "", NULL, Spin, N_("Animation Speed (high = slow):") },
{ 0,  1, 5, NULL, (void*) &appData.zoom, "", NULL, Spin, N_("Zoom factor in Evaluation Graph:") },
{ 0,  0, 0, NULL, (void*) &GeneralOptionsOK, "", NULL, EndMark , "" }
};

void
OptionsProc ()
{
   oldPonder = appData.ponderNextMove;
   oldShow = appData.showCoords; oldBlind = appData.blindfold;
   GenericPopUp(generalOptions, _("General Options"), TransientDlg);
}

//---------------------------------------------- New Variant ------------------------------------------------

static void Pick P((int n));

static Option variantDescriptors[] = {
{ VariantNormal, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("normal")},
{ VariantFairy, 1, 135, NULL, (void*) &Pick, "#BFBFBF", NULL, Button, N_("fairy")},
{ VariantFischeRandom, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("FRC")},
{ VariantSChess, 1, 135, NULL, (void*) &Pick, "#FFBFBF", NULL, Button, N_("Seirawan")},
{ VariantWildCastle, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("wild castle")},
{ VariantSuper, 1, 135, NULL, (void*) &Pick, "#FFBFBF", NULL, Button, N_("Superchess")},
{ VariantNoCastle, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("no castle")},
{ VariantCrazyhouse, 1, 135, NULL, (void*) &Pick, "#FFBFBF", NULL, Button, N_("crazyhouse")},
{ VariantKnightmate, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("knightmate")},
{ VariantBughouse, 1, 135, NULL, (void*) &Pick, "#FFBFBF", NULL, Button, N_("bughouse")},
{ VariantBerolina, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("berolina")},
{ VariantShogi, 1, 135, NULL, (void*) &Pick, "#BFFFFF", NULL, Button, N_("shogi (9x9)")},
{ VariantCylinder, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("cylinder")},
{ VariantXiangqi, 1, 135, NULL, (void*) &Pick, "#BFFFFF", NULL, Button, N_("xiangqi (9x10)")},
{ VariantShatranj, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("shatranj")},
{ VariantCourier, 1, 135, NULL, (void*) &Pick, "#BFFFBF", NULL, Button, N_("courier (12x8)")},
{ VariantMakruk, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("makruk")},
{ VariantGreat, 1, 135, NULL, (void*) &Pick, "#BFBFFF", NULL, Button, N_("Great Shatranj (10x8)")},
{ VariantAtomic, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("atomic")},
{ VariantFalcon, 1, 135, NULL, (void*) &Pick, "#BFBFFF", NULL, Button, N_("falcon (10x8)")},
{ VariantTwoKings, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("two kings")},
{ VariantCapablanca, 1, 135, NULL, (void*) &Pick, "#BFBFFF", NULL, Button, N_("Capablanca (10x8)")},
{ Variant3Check, 0, 135, NULL, (void*) &Pick, "#FFFFFF", NULL, Button, N_("3-checks")},
{ VariantGothic, 1, 135, NULL, (void*) &Pick, "#BFBFFF", NULL, Button, N_("Gothic (10x8)")},
{ VariantSuicide, 0, 135, NULL, (void*) &Pick, "#FFFFBF", NULL, Button, N_("suicide")},
{ VariantJanus, 1, 135, NULL, (void*) &Pick, "#BFBFFF", NULL, Button, N_("janus (10x8)")},
{ VariantGiveaway, 0, 135, NULL, (void*) &Pick, "#FFFFBF", NULL, Button, N_("give-away")},
{ VariantCapaRandom, 1, 135, NULL, (void*) &Pick, "#BFBFFF", NULL, Button, N_("CRC (10x8)")},
{ VariantLosers, 0, 135, NULL, (void*) &Pick, "#FFFFBF", NULL, Button, N_("losers")},
{ VariantGrand, 1, 135, NULL, (void*) &Pick, "#5070FF", NULL, Button, N_("grand (10x10)")},
{ VariantSpartan, 0, 135, NULL, (void*) &Pick, "#FF0000", NULL, Button, N_("Spartan")},
{ 0, 0, 0, NULL, NULL, NULL, NULL, Label, N_("Board size ( -1 = default for selected variant):")},
{ 0, -1, BOARD_RANKS-1, NULL, (void*) &appData.NrRanks, "", NULL, Spin, N_("Number of Board Ranks:") },
{ 0, -1, BOARD_FILES, NULL, (void*) &appData.NrFiles, "", NULL, Spin, N_("Number of Board Files:") },
{ 0, -1, BOARD_RANKS-1, NULL, (void*) &appData.holdingsSize, "", NULL, Spin, N_("Holdings Size:") },
{ 0, 0, 0, NULL, NULL, NULL, NULL, Label,
				N_("WARNING: variants with un-orthodox\n"
				  "pieces only have built-in bitmaps\n"
				  "for -boardSize middling, bulky and\n"
				  "petite, and substitute king or amazon\n"
				  "for missing bitmaps. (See manual.)")},
{ 0, 2, 0, NULL, NULL, "", NULL, EndMark , "" }
};

static void
Pick (int n)
{
	VariantClass v = variantDescriptors[n].value;
	if(!appData.noChessProgram) {
	    char *name = VariantName(v), buf[MSG_SIZ];
	    if (first.protocolVersion > 1 && StrStr(first.variants, name) == NULL) {
		/* [HGM] in protocol 2 we check if variant is suported by engine */
	      snprintf(buf, MSG_SIZ,  _("Variant %s not supported by %s"), name, first.tidy);
		DisplayError(buf, 0);
		return; /* ignore OK if first engine does not support it */
	    } else
	    if (second.initDone && second.protocolVersion > 1 && StrStr(second.variants, name) == NULL) {
	      snprintf(buf, MSG_SIZ,  _("Warning: second engine (%s) does not support this!"), second.tidy);
		DisplayError(buf, 0);   /* use of second engine is optional; only warn user */
	    }
	}

	GenericReadout(variantDescriptors, -1); // make sure ranks and file settings are read

	gameInfo.variant = v;
	appData.variant = VariantName(v);

	shuffleOpenings = FALSE; /* [HGM] shuffle: possible shuffle reset when we switch */
	startedFromPositionFile = FALSE; /* [HGM] loadPos: no longer valid in new variant */
	appData.pieceToCharTable = NULL;
	appData.pieceNickNames = "";
	appData.colorNickNames = "";
	Reset(True, True);
        PopDown(TransientDlg);
        return;
}

void
NewVariantProc ()
{
   GenericPopUp(variantDescriptors, _("New Variant"), TransientDlg);
}

//------------------------------------------- Common Engine Options -------------------------------------

static int oldCores;

static int
CommonOptionsOK (int n)
{
	int newPonder = appData.ponderNextMove;
	// make sure changes are sent to first engine by re-initializing it
	// if it was already started pre-emptively at end of previous game
	if(gameMode == BeginningOfGame) Reset(True, True); else {
	    // Some changed setting need immediate sending always.
	    if(oldCores != appData.smpCores)
		NewSettingEvent(False, &(first.maxCores), "cores", appData.smpCores);
	    appData.ponderNextMove = oldPonder;
	    PonderNextMoveEvent(newPonder);
	}
	return 1;
}

static Option commonEngineOptions[] = {
{ 0,     0, 0, NULL, (void*) &appData.ponderNextMove, "", NULL, CheckBox, N_("Ponder Next Move") },
{ 0,  0, 1000, NULL, (void*) &appData.smpCores, "", NULL, Spin, N_("Maximum Number of CPUs per Engine:") },
{ 0,     0, 0, NULL, (void*) &appData.polyglotDir, "", NULL, PathName, N_("Polygot Directory:") },
{ 0, 0, 16000, NULL, (void*) &appData.defaultHashSize, "", NULL, Spin, N_("Hash-Table Size (MB):") },
{ 0,     0, 0, NULL, (void*) &appData.defaultPathEGTB, "", NULL, PathName, N_("Nalimov EGTB Path:") },
{ 0,  0, 1000, NULL, (void*) &appData.defaultCacheSizeEGTB, "", NULL, Spin, N_("EGTB Cache Size (MB):") },
{ 0,     0, 0, NULL, (void*) &appData.usePolyglotBook, "", NULL, CheckBox, N_("Use GUI Book") },
{ 0,     0, 0, NULL, (void*) &appData.polyglotBook, ".bin", NULL, FileName, N_("Opening-Book Filename:") },
{ 0,   0, 100, NULL, (void*) &appData.bookDepth, "", NULL, Spin, N_("Book Depth (moves):") },
{ 0,   0, 100, NULL, (void*) &appData.bookStrength, "", NULL, Spin, N_("Book Variety (0) vs. Strength (100):") },
{ 0,     0, 0, NULL, (void*) &appData.firstHasOwnBookUCI, "", NULL, CheckBox, N_("Engine #1 Has Own Book") },
{ 0,     0, 0, NULL, (void*) &appData.secondHasOwnBookUCI, "", NULL, CheckBox, N_("Engine #2 Has Own Book          ") },
{ 0,     1, 0, NULL, (void*) &CommonOptionsOK, "", NULL, EndMark , "" }
};

void
UciMenuProc ()
{
   oldCores = appData.smpCores;
   oldPonder = appData.ponderNextMove;
   GenericPopUp(commonEngineOptions, _("Common Engine Settings"), TransientDlg);
}

//------------------------------------------ Adjudication Options --------------------------------------

static Option adjudicationOptions[] = {
{ 0, 0,    0, NULL, (void*) &appData.checkMates, "", NULL, CheckBox, N_("Detect all Mates") },
{ 0, 0,    0, NULL, (void*) &appData.testClaims, "", NULL, CheckBox, N_("Verify Engine Result Claims") },
{ 0, 0,    0, NULL, (void*) &appData.materialDraws, "", NULL, CheckBox, N_("Draw if Insufficient Mating Material") },
{ 0, 0,    0, NULL, (void*) &appData.trivialDraws, "", NULL, CheckBox, N_("Adjudicate Trivial Draws (3-Move Delay)") },
{ 0, 0,  100, NULL, (void*) &appData.ruleMoves, "", NULL, Spin, N_("N-Move Rule:") },
{ 0, 0,    6, NULL, (void*) &appData.drawRepeats, "", NULL, Spin, N_("N-fold Repeats:") },
{ 0, 0, 1000, NULL, (void*) &appData.adjudicateDrawMoves, "", NULL, Spin, N_("Draw after N Moves Total:") },
{ 0,-5000, 0, NULL, (void*) &appData.adjudicateLossThreshold, "", NULL, Spin, N_("Win / Loss Threshold:") },
{ 0, 0,    0, NULL, (void*) &first.scoreIsAbsolute, "", NULL, CheckBox, N_("Negate Score of Engine #1") },
{ 0, 0,    0, NULL, (void*) &second.scoreIsAbsolute, "", NULL, CheckBox, N_("Negate Score of Engine #2") },
{ 0, 1,    0, NULL, NULL, "", NULL, EndMark , "" }
};

void
EngineMenuProc ()
{
   GenericPopUp(adjudicationOptions, _("Adjudicate non-ICS Games"), TransientDlg);
}

//--------------------------------------------- ICS Options ---------------------------------------------

static int
IcsOptionsOK (int n)
{
    ParseIcsTextColors();
    return 1;
}

Option icsOptions[] = {
{ 0, 0, 0, NULL, (void*) &appData.autoKibitz, "",  NULL, CheckBox, N_("Auto-Kibitz") },
{ 0, 0, 0, NULL, (void*) &appData.autoComment, "", NULL, CheckBox, N_("Auto-Comment") },
{ 0, 0, 0, NULL, (void*) &appData.autoObserve, "", NULL, CheckBox, N_("Auto-Observe") },
{ 0, 0, 0, NULL, (void*) &appData.autoRaiseBoard, "", NULL, CheckBox, N_("Auto-Raise Board") },
{ 0, 0, 0, NULL, (void*) &appData.bgObserve, "",   NULL, CheckBox, N_("Background Observe while Playing") },
{ 0, 0, 0, NULL, (void*) &appData.dualBoard, "",   NULL, CheckBox, N_("Dual Board for Background-Observed Game") },
{ 0, 0, 0, NULL, (void*) &appData.getMoveList, "", NULL, CheckBox, N_("Get Move List") },
{ 0, 0, 0, NULL, (void*) &appData.quietPlay, "",   NULL, CheckBox, N_("Quiet Play") },
{ 0, 0, 0, NULL, (void*) &appData.seekGraph, "",   NULL, CheckBox, N_("Seek Graph") },
{ 0, 0, 0, NULL, (void*) &appData.autoRefresh, "", NULL, CheckBox, N_("Auto-Refresh Seek Graph") },
{ 0, 0, 0, NULL, (void*) &appData.premove, "",     NULL, CheckBox, N_("Premove") },
{ 0, 0, 0, NULL, (void*) &appData.premoveWhite, "", NULL, CheckBox, N_("Premove for White") },
{ 0, 0, 0, NULL, (void*) &appData.premoveWhiteText, "", NULL, TextBox, N_("First White Move:") },
{ 0, 0, 0, NULL, (void*) &appData.premoveBlack, "", NULL, CheckBox, N_("Premove for Black") },
{ 0, 0, 0, NULL, (void*) &appData.premoveBlackText, "", NULL, TextBox, N_("First Black Move:") },
{ 0, 0, 0, NULL, NULL, NULL, NULL, Break, "" },
{ 0, 0, 0, NULL, (void*) &appData.icsAlarm, "", NULL, CheckBox, N_("Alarm") },
{ 0, 0, 100000000, NULL, (void*) &appData.icsAlarmTime, "", NULL, Spin, N_("Alarm Time (msec):") },
//{ 0, 0, 0, NULL, (void*) &appData.chatBoxes, "", NULL, TextBox, N_("Startup Chat Boxes:") },
{ 0, 0, 0, NULL, (void*) &appData.colorize, "", NULL, CheckBox, N_("Colorize Messages") },
{ 0, 0, 0, NULL, (void*) &appData.colorShout, "", NULL, TextBox, N_("Shout Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorSShout, "", NULL, TextBox, N_("S-Shout Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorChannel1, "", NULL, TextBox, N_("Channel #1 Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorChannel, "", NULL, TextBox, N_("Other Channel Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorKibitz, "", NULL, TextBox, N_("Kibitz Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorTell, "", NULL, TextBox, N_("Tell Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorChallenge, "", NULL, TextBox, N_("Challenge Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorRequest, "", NULL, TextBox, N_("Request Text Colors:") },
{ 0, 0, 0, NULL, (void*) &appData.colorSeek, "", NULL, TextBox, N_("Seek Text Colors:") },
{ 0, 0, 0, NULL, (void*) &IcsOptionsOK, "", NULL, EndMark , "" }
};

void
IcsOptionsProc ()
{
   GenericPopUp(icsOptions, _("ICS Options"), TransientDlg);
}

//-------------------------------------------- Load Game Options ---------------------------------

static char *modeNames[] = { N_("Exact position match"), N_("Shown position is subset"), N_("Same material with exactly same Pawn chain"), 
		      N_("Same material"), N_("Material range (top board half optional)"), N_("Material difference (optional stuff balanced)"), NULL };
static char *modeValues[] = { "1", "2", "3", "4", "5", "6" };
static char *searchMode;

static int
LoadOptionsOK ()
{
    appData.searchMode = atoi(searchMode);
    return 1;
}

static Option loadOptions[] = {
{ 0, 0, 0, NULL, (void*) &appData.autoDisplayTags, "", NULL, CheckBox, N_("Auto-Display Tags") },
{ 0, 0, 0, NULL, (void*) &appData.autoDisplayComment, "", NULL, CheckBox, N_("Auto-Display Comment") },
{ 0, 0, 0, NULL, NULL, NULL, NULL, Label, N_("Auto-Play speed of loaded games\n(0 = instant, -1 = off):") },
{ 0, -1, 10000000, NULL, (void*) &appData.timeDelay, "", NULL, Fractional, N_("Seconds per Move:") },
{   0,  0,    0, NULL, NULL, NULL, NULL, Label,  N_("\noptions to use in game-viewer mode:") },
{ 0, 0, 300, NULL, (void*) &appData.viewerOptions, "", NULL, TextBox,  "" },
{   0,  0,    0, NULL, NULL, NULL, NULL, Label,  N_("\nThresholds for position filtering in game list:") },
{ 0, 0, 5000, NULL, (void*) &appData.eloThreshold1, "", NULL, Spin, N_("Elo of strongest player at least:") },
{ 0, 0, 5000, NULL, (void*) &appData.eloThreshold2, "", NULL, Spin, N_("Elo of weakest player at least:") },
{ 0, 0, 5000, NULL, (void*) &appData.dateThreshold, "", NULL, Spin, N_("No games before year:") },
{ 0, 1, 50, NULL, (void*) &appData.stretch, "", NULL, Spin, N_("Minimum nr consecutive positions:") },
{ 1, 0, 180, NULL, (void*) &searchMode, (char*) modeNames, modeValues, ComboBox, N_("Search mode:") },
{ 0, 0, 0, NULL, (void*) &appData.ignoreColors, "", NULL, CheckBox, N_("Also match reversed colors") },
{ 0, 0, 0, NULL, (void*) &appData.findMirror, "", NULL, CheckBox, N_("Also match left-right flipped position") },
{ 0,  0, 0, NULL, (void*) &LoadOptionsOK, "", NULL, EndMark , "" }
};

void
LoadOptionsProc ()
{
   ASSIGN(searchMode, modeValues[appData.searchMode-1]);
   GenericPopUp(loadOptions, _("Load Game Options"), TransientDlg);
}

//------------------------------------------- Save Game Options --------------------------------------------

static Option saveOptions[] = {
{ 0, 0, 0, NULL, (void*) &appData.autoSaveGames, "", NULL, CheckBox, N_("Auto-Save Games") },
{ 0, 0, 0, NULL, (void*) &appData.saveGameFile, ".pgn", NULL, FileName,  N_("Save Games on File:") },
{ 0, 0, 0, NULL, (void*) &appData.savePositionFile, ".fen", NULL, FileName,  N_("Save Final Positions on File:") },
{ 0, 0, 0, NULL, (void*) &appData.pgnEventHeader, "", NULL, TextBox,  N_("PGN Event Header:") },
{ 0, 0, 0, NULL, (void*) &appData.oldSaveStyle, "", NULL, CheckBox, N_("Old Save Style (as opposed to PGN)") },
{ 0, 0, 0, NULL, (void*) &appData.numberTag, "", NULL, CheckBox, N_("Include Number Tag in tourney PGN") },
{ 0, 0, 0, NULL, (void*) &appData.saveExtendedInfoInPGN, "", NULL, CheckBox, N_("Save Score/Depth Info in PGN") },
{ 0, 0, 0, NULL, (void*) &appData.saveOutOfBookInfo, "", NULL, CheckBox, N_("Save Out-of-Book Info in PGN           ") },
{ 0, 1, 0, NULL, NULL, "", NULL, EndMark , "" }
};

void
SaveOptionsProc ()
{
   GenericPopUp(saveOptions, _("Save Game Options"), TransientDlg);
}

//----------------------------------------------- Sound Options ---------------------------------------------

static void Test P((int n));
static char *trialSound;

static char *soundNames[] = {
	N_("No Sound"),
	N_("Default Beep"),
	N_("Above WAV File"),
	N_("Car Horn"),
	N_("Cymbal"),
	N_("Ding"),
	N_("Gong"),
	N_("Laser"),
	N_("Penalty"),
	N_("Phone"),
	N_("Pop"),
	N_("Slap"),
	N_("Wood Thunk"),
	NULL,
	N_("User File")
};

static char *soundFiles[] = { // sound files corresponding to above names
	"",
	"$",
	NULL, // kludge alert: as first thing in the dialog readout this is replaced with the user-given .WAV filename
	"honkhonk.wav",
	"cymbal.wav",
	"ding1.wav",
	"gong.wav",
	"laser.wav",
	"penalty.wav",
	"phone.wav",
	"pop2.wav",
	"slap.wav",
	"woodthunk.wav",
	NULL,
	NULL
};

static Option soundOptions[] = {
{ 0, 0, 0, NULL, (void*) &appData.soundProgram, "", NULL, TextBox, N_("Sound Program:") },
{ 0, 0, 0, NULL, (void*) &appData.soundDirectory, "", NULL, PathName, N_("Sounds Directory:") },
{ 0, 0, 0, NULL, (void*) (soundFiles+2) /* kludge! */, ".wav", NULL, FileName, N_("User WAV File:") },
{ 0, 0, 0, NULL, (void*) &trialSound, (char*) soundNames, soundFiles, ComboBox, N_("Try-Out Sound:") },
{ 0, 1, 0, NULL, (void*) &Test, NULL, NULL, Button, N_("Play") },
{ 0, 0, 0, NULL, (void*) &appData.soundMove, (char*) soundNames, soundFiles, ComboBox, N_("Move:") },
{ 0, 0, 0, NULL, (void*) &appData.soundIcsWin, (char*) soundNames, soundFiles, ComboBox, N_("Win:") },
{ 0, 0, 0, NULL, (void*) &appData.soundIcsLoss, (char*) soundNames, soundFiles, ComboBox, N_("Lose:") },
{ 0, 0, 0, NULL, (void*) &appData.soundIcsDraw, (char*) soundNames, soundFiles, ComboBox, N_("Draw:") },
{ 0, 0, 0, NULL, (void*) &appData.soundIcsUnfinished, (char*) soundNames, soundFiles, ComboBox, N_("Unfinished:") },
{ 0, 0, 0, NULL, (void*) &appData.soundIcsAlarm, (char*) soundNames, soundFiles, ComboBox, N_("Alarm:") },
{ 0, 0, 0, NULL, (void*) &appData.soundShout, (char*) soundNames, soundFiles, ComboBox, N_("Shout:") },
{ 0, 0, 0, NULL, (void*) &appData.soundSShout, (char*) soundNames, soundFiles, ComboBox, N_("S-Shout:") },
{ 0, 0, 0, NULL, (void*) &appData.soundChannel, (char*) soundNames, soundFiles, ComboBox, N_("Channel:") },
{ 0, 0, 0, NULL, (void*) &appData.soundChannel1, (char*) soundNames, soundFiles, ComboBox, N_("Channel 1:") },
{ 0, 0, 0, NULL, (void*) &appData.soundTell, (char*) soundNames, soundFiles, ComboBox, N_("Tell:") },
{ 0, 0, 0, NULL, (void*) &appData.soundKibitz, (char*) soundNames, soundFiles, ComboBox, N_("Kibitz:") },
{ 0, 0, 0, NULL, (void*) &appData.soundChallenge, (char*) soundNames, soundFiles, ComboBox, N_("Challenge:") },
{ 0, 0, 0, NULL, (void*) &appData.soundRequest, (char*) soundNames, soundFiles, ComboBox, N_("Request:") },
{ 0, 0, 0, NULL, (void*) &appData.soundSeek, (char*) soundNames, soundFiles, ComboBox, N_("Seek:") },
{ 0, 1, 0, NULL, NULL, "", NULL, EndMark , "" }
};

static void
Test (int n)
{
    GenericReadout(soundOptions, 2);
    if(soundFiles[values[3]]) PlaySound(soundFiles[values[3]]);
}

void
SoundOptionsProc ()
{
   free(soundFiles[2]);
   soundFiles[2] = strdup("*");
   GenericPopUp(soundOptions, _("Sound Options"), TransientDlg);
}

//--------------------------------------------- Board Options --------------------------------------

static void DefColor P((int n));
static void AdjustColor P((int i));

static int
BoardOptionsOK (int n)
{
    if(appData.overrideLineGap >= 0) lineGap = appData.overrideLineGap; else lineGap = defaultLineGap;
    useImages = useImageSqs = 0;
    InitDrawingParams();
    InitDrawingSizes(-1, 0);
    DrawPosition(True, NULL);
    return 1;
}

static Option boardOptions[] = {
{ 0,   0, 70, NULL, (void*) &appData.whitePieceColor, "", NULL, TextBox, N_("White Piece Color:") },
{ 1000, 1, 0, NULL, (void*) &DefColor, NULL, (char**) "#FFFFCC", Button, "      " },
/* TRANSLATORS: R = single letter for the color red */
{    1, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("R") },
/* TRANSLATORS: G = single letter for the color green */
{    2, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("G") },
/* TRANSLATORS: B = single letter for the color blue */
{    3, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("B") },
/* TRANSLATORS: D = single letter to make a color darker */
{    4, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("D") },
{ 0,   0, 70, NULL, (void*) &appData.blackPieceColor, "", NULL, TextBox, N_("Black Piece Color:") },
{ 1000, 1, 0, NULL, (void*) &DefColor, NULL, (char**) "#202020", Button, "      " },
{    1, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("R") },
{    2, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("G") },
{    3, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("B") },
{    4, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("D") },
{ 0,   0, 70, NULL, (void*) &appData.lightSquareColor, "", NULL, TextBox, N_("Light Square Color:") },
{ 1000, 1, 0, NULL, (void*) &DefColor, NULL, (char**) "#C8C365", Button, "      " },
{    1, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("R") },
{    2, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("G") },
{    3, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("B") },
{    4, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("D") },
{ 0,   0, 70, NULL, (void*) &appData.darkSquareColor, "", NULL, TextBox, N_("Dark Square Color:") },
{ 1000, 1, 0, NULL, (void*) &DefColor, NULL, (char**) "#77A26D", Button, "      " },
{    1, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("R") },
{    2, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("G") },
{    3, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("B") },
{    4, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("D") },
{ 0,   0, 70, NULL, (void*) &appData.highlightSquareColor, "", NULL, TextBox, N_("Highlight Color:") },
{ 1000, 1, 0, NULL, (void*) &DefColor, NULL, (char**) "#FFFF00", Button, "      " },
{    1, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("R") },
{    2, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("G") },
{    3, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("B") },
{    4, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("D") },
{ 0,   0, 70, NULL, (void*) &appData.premoveHighlightColor, "", NULL, TextBox, N_("Premove Highlight Color:") },
{ 1000, 1, 0, NULL, (void*) &DefColor, NULL, (char**) "#FF0000", Button, "      " },
{    1, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("R") },
{    2, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("G") },
{    3, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("B") },
{    4, 1, 0, NULL, (void*) &AdjustColor, NULL, NULL, Button, N_("D") },
{ 0, 0, 0, NULL, (void*) &appData.upsideDown, "", NULL, CheckBox, N_("Flip Pieces Shogi Style        (Colored buttons restore default)") },
//{ 0, 0, 0, NULL, (void*) &appData.allWhite, "", NULL, CheckBox, N_("Use Outline Pieces for Black") },
{ 0, 0, 0, NULL, (void*) &appData.monoMode, "", NULL, CheckBox, N_("Mono Mode") },
{ 0,-1, 5, NULL, (void*) &appData.overrideLineGap, "", NULL, Spin, N_("Line Gap ( -1 = default for board size):") },
{ 0, 0, 0, NULL, (void*) &appData.useBitmaps, "", NULL, CheckBox, N_("Use Board Textures") },
{ 0, 0, 0, NULL, (void*) &appData.liteBackTextureFile, ".xpm", NULL, FileName, N_("Light-Squares Texture File:") },
{ 0, 0, 0, NULL, (void*) &appData.darkBackTextureFile, ".xpm", NULL, FileName, N_("Dark-Squares Texture File:") },
{ 0, 0, 0, NULL, (void*) &appData.bitmapDirectory, "", NULL, PathName, N_("Directory with Bitmap Pieces:") },
{ 0, 0, 0, NULL, (void*) &appData.pixmapDirectory, "", NULL, PathName, N_("Directory with Pixmap Pieces:") },
{ 0, 0, 0, NULL, (void*) &BoardOptionsOK, "", NULL, EndMark , "" }
};

static void
SetColorText (int n, char *buf)
{
    SetWidgetText(&boardOptions[n-1], buf, TransientDlg);
    SetColor(buf, &boardOptions[n]);
}

static void
DefColor (int n)
{
    SetColorText(n, (char*) boardOptions[n].choice);
}

void
RefreshColor (int source, int n)
{
    int col, j, r, g, b, step = 10;
    char *s, buf[MSG_SIZ]; // color string
    GetWidgetText(&boardOptions[source], &s);
    if(sscanf(s, "#%x", &col) != 1) return;   // malformed
    b = col & 0xFF; g = col & 0xFF00; r = col & 0xFF0000;
    switch(n) {
	case 1: r += 0x10000*step;break;
	case 2: g += 0x100*step;  break;
	case 3: b += step;        break;
	case 4: r -= 0x10000*step; g -= 0x100*step; b -= step; break;
    }
    if(r < 0) r = 0; if(g < 0) g = 0; if(b < 0) b = 0;
    if(r > 0xFF0000) r = 0xFF0000; if(g > 0xFF00) g = 0xFF00; if(b > 0xFF) b = 0xFF;
    col = r | g | b;
    snprintf(buf, MSG_SIZ, "#%06x", col);
    for(j=1; j<7; j++) if(buf[j] >= 'a') buf[j] -= 32; // capitalize
    SetColorText(source+1, buf);
}

static void
AdjustColor (int i)
{
    int n = boardOptions[i].value;
    RefreshColor(i-n-1, n);
}

void
BoardOptionsProc ()
{
   GenericPopUp(boardOptions, _("Board Options"), TransientDlg);
}

//-------------------------------------------- ICS Text Menu Options ------------------------------

Option textOptions[100];
static void PutText P((char *text, int pos));

void
SendString (char *p)
{
    char buf[MSG_SIZ], *q;
    if(q = strstr(p, "$input")) {
	if(!shellUp[TextMenuDlg]) return;
	strncpy(buf, p, MSG_SIZ);
	strncpy(buf + (q-p), q+6, MSG_SIZ-(q-p));
	PutText(buf, q-p);
	return;
    }
    snprintf(buf, MSG_SIZ, "%s\n", p);
    SendToICS(buf);
}

void
IcsTextProc ()
{
   int i=0, j;
   char *p, *q, *r;
   if((p = icsTextMenuString) == NULL) return;
   do {
	q = r = p; while(*p && *p != ';') p++;
	for(j=0; j<p-q; j++) textOptions[i].name[j] = *r++;
	textOptions[i].name[j++] = 0;
	if(!*p) break;
	if(*++p == '\n') p++; // optional linefeed after button-text terminating semicolon
	q = p;
	textOptions[i].choice = (char**) (r = textOptions[i].name + j);
	while(*p && (*p != ';' || p[1] != '\n')) textOptions[i].name[j++] = *p++;
	textOptions[i].name[j++] = 0;
	if(*p) p += 2;
	textOptions[i].max = 135;
	textOptions[i].min = i&1;
	textOptions[i].handle = NULL;
	textOptions[i].target = &SendText;
	textOptions[i].textValue = strstr(r, "$input") ? "#80FF80" : strstr(r, "$name") ? "#FF8080" : "#FFFFFF";
	textOptions[i].type = Button;
   } while(++i < 99 && *p);
   if(i == 0) return;
   textOptions[i].type = EndMark;
   textOptions[i].target = NULL;
   textOptions[i].min = 2;
   MarkMenu("ICStex", TextMenuDlg);
   GenericPopUp(textOptions, _("ICS text menu"), TextMenuDlg);
}

//---------------------------------------------------- Edit Comment -----------------------------------

static char *commentText;
static int commentIndex;
static void ClearComment P((int n));
static void SaveChanges P((int n));

static int
NewComCallback (int n)
{
    ReplaceComment(commentIndex, commentText);
    return 1;
}

Option commentOptions[] = {
{ 0xD, 200, 250, NULL, (void*) &commentText, "", NULL, TextBox, "" },
{   0,  0,   50, NULL, (void*) &ClearComment, NULL, NULL, Button, N_("clear") },
{   0,  1,  100, NULL, (void*) &SaveChanges, NULL, NULL, Button, N_("save changes") },
{   0,  1,    0, NULL, (void*) &NewComCallback, "", NULL, EndMark , "" }
};

static void
SaveChanges (int n)
{
    GenericReadout(commentOptions, 0);
    ReplaceComment(commentIndex, commentText);
}

static void
ClearComment (int n)
{
    SetWidgetText(&commentOptions[0], "", CommentDlg);
}

void
NewCommentPopup (char *title, char *text, int index)
{
    if(DialogExists(CommentDlg)) { // if already exists, alter title and content
	SetDialogTitle(CommentDlg, title);
	SetWidgetText(&commentOptions[0], text, CommentDlg);
    }
    if(commentText) free(commentText); commentText = strdup(text);
    commentIndex = index;
    MarkMenu("Show Comments", CommentDlg);
    if(GenericPopUp(commentOptions, title, CommentDlg))
	AddHandler(&commentOptions[0], 1);
}

void
EditCommentProc ()
{
    int j;
    if (PopDown(CommentDlg)) { // popdown succesful
	MarkMenuItem("Edit Comment", False);
	MarkMenuItem("Show Comments", False);
    } else // was not up
	EditCommentEvent();
}

//------------------------------------------------------ Edit Tags ----------------------------------

static void changeTags P((int n));
static char *tagsText;

static int
NewTagsCallback (int n)
{
    ReplaceTags(tagsText, &gameInfo);
    return 1;
}

static Option tagsOptions[] = {
{   0,  0,    0, NULL, NULL, NULL, NULL, Label,  "" },
{ 0xD, 200, 200, NULL, (void*) &tagsText, "", NULL, TextBox, "" },
{   0,  0,  100, NULL, (void*) &changeTags, NULL, NULL, Button, N_("save changes") },
{   0,  1,    0, NULL, (void*) &NewTagsCallback, "", NULL, EndMark , "" }
};

static void
changeTags (int n)
{
    GenericReadout(tagsOptions, 1);
    if(bookUp) SaveToBook(tagsText); else
    ReplaceTags(tagsText, &gameInfo);
}

void
NewTagsPopup (char *text, char *msg)
{
    char *title = bookUp ? _("Edit book") : _("Tags");

    if(DialogExists(TagsDlg)) { // if already exists, alter title and content
	SetWidgetText(&tagsOptions[1], text, TagsDlg);
	SetDialogTitle(TagsDlg, title);
    }
    if(tagsText) free(tagsText); tagsText = strdup(text);
    tagsOptions[0].textValue = msg;
    MarkMenu("Show Tags", TagsDlg);
    GenericPopUp(tagsOptions, title, TagsDlg);
}

//---------------------------------------------- ICS Input Box ----------------------------------

char *icsText;

Option boxOptions[] = {
{   0, 30,  400, NULL, (void*) &icsText, "", NULL, TextBox, "" },
{   0,  3,    0, NULL, NULL, "", NULL, EndMark , "" }
};

static void
PutText (char *text, int pos)
{
    char buf[MSG_SIZ], *p;

    if(strstr(text, "$add ") == text) {
	GetWidgetText(&boxOptions[0], &p);
	snprintf(buf, MSG_SIZ, "%s%s", p, text+5); text = buf;
	pos += strlen(p) - 5;
    }
    SetWidgetText(&boxOptions[0], text, TextMenuDlg);
    SetInsertPos(&boxOptions[0], pos);
}

void
InputBoxPopup ()
{
    MarkMenu("ICS Input Box", InputBoxDlg);
    if(GenericPopUp(boxOptions, _("ICS input box"), InputBoxDlg))
	AddHandler(&boxOptions[0], 3);
}

void
IcsInputBoxProc ()
{
    if (!PopDown(InputBoxDlg)) ICSInputBoxPopUp();
}

//--------------------------------------------- Move Type In ------------------------------------------

void
PopUpMoveDialog (char firstchar)
{
    static char buf[2];
    buf[0] = firstchar; icsText = buf;
    if(GenericPopUp(boxOptions, _("Type a move"), TransientDlg))
	AddHandler(&boxOptions[0], 2);
}

void
BoxAutoPopUp (char *buf)
{
	if(appData.icsActive) { // text typed to board in ICS mode: divert to ICS input box
	    if(DialogExists(InputBoxDlg)) { // box already exists: append to current contents
		char *p, newText[MSG_SIZ];
		GetWidgetText(&boxOptions[0], &p);
		snprintf(newText, MSG_SIZ, "%s%c", p, *buf);
		SetWidgetText(&boxOptions[0], newText, InputBoxDlg);
		if(shellUp[InputBoxDlg]) HardSetFocus (&boxOptions[0]); //why???
	    } else icsText = buf; // box did not exist: make sure it pops up with char in it
	    InputBoxPopup();
	} else PopUpMoveDialog(*buf);
}

//------------------------------------------ Engine Settings ------------------------------------

void
SettingsPopUp (ChessProgramState *cps)
{
   currentCps = cps;
   GenericPopUp(cps->option, _("Engine Settings"), TransientDlg);
}

void
FirstSettingsProc ()
{
    SettingsPopUp(&first);
}

void
SecondSettingsProc ()
{
   if(WaitForEngine(&second, SettingsMenuIfReady)) return;
   SettingsPopUp(&second);
}

//----------------------------------------------- Load Engine --------------------------------------

char *engineDir, *engineLine, *nickName, *params;
Boolean isUCI, hasBook, storeVariant, v1, addToList, useNick;
static char *engineNr[] = { N_("First Engine"), N_("Second Engine"), NULL };

static int
InstallOK (int n)
{
    PopDown(TransientDlg); // early popdown, to allow FreezeUI to instate grab
    if(engineChoice[0] == engineNr[0][0])  Load(&first, 0); else Load(&second, 1);
    return 1;
}

static Option installOptions[] = {
{   0,  NO_GETTEXT, 0, NULL, (void*) &engineLine, (char*) engineMnemonic, engineList, ComboBox, N_("Select engine from list:") },
{   0,  0,    0, NULL, NULL, NULL, NULL, Label, N_("or specify one below:") },
{   0,  0,    0, NULL, (void*) &nickName, NULL, NULL, TextBox, N_("Nickname (optional):") },
{   0,  0,    0, NULL, (void*) &useNick, NULL, NULL, CheckBox, N_("Use nickname in PGN player tags of engine-engine games") },
{   0,  0,    0, NULL, (void*) &engineDir, NULL, NULL, PathName, N_("Engine Directory:") },
{   0,  0,    0, NULL, (void*) &engineName, NULL, NULL, FileName, N_("Engine Command:") },
{   0,  0,    0, NULL, NULL, NULL, NULL, Label, N_("(Directory will be derived from engine path when empty)") },
{   0,  0,    0, NULL, (void*) &isUCI, NULL, NULL, CheckBox, N_("UCI") },
{   0,  0,    0, NULL, (void*) &v1, NULL, NULL, CheckBox, N_("WB protocol v1 (do not wait for engine features)") },
{   0,  0,    0, NULL, (void*) &hasBook, NULL, NULL, CheckBox, N_("Must not use GUI book") },
{   0,  0,    0, NULL, (void*) &addToList, NULL, NULL, CheckBox, N_("Add this engine to the list") },
{   0,  0,    0, NULL, (void*) &storeVariant, NULL, NULL, CheckBox, N_("Force current variant with this engine") },
{   0,  0,    0, NULL, (void*) &engineChoice, (char*) engineNr, engineNr, ComboBox, N_("Load mentioned engine as") },
{   0,  1,    0, NULL, (void*) &InstallOK, "", NULL, EndMark , "" }
};

void
LoadEngineProc ()
{
   isUCI = storeVariant = v1 = useNick = False; addToList = hasBook = True; // defaults
   if(engineChoice) free(engineChoice); engineChoice = strdup(engineNr[0]);
   if(engineLine)   free(engineLine);   engineLine = strdup("");
   if(engineDir)    free(engineDir);    engineDir = strdup("");
   if(nickName)     free(nickName);     nickName = strdup("");
   if(params)       free(params);       params = strdup("");
   NamesToList(firstChessProgramNames, engineList, engineMnemonic, "all");
   GenericPopUp(installOptions, _("Load engine"), TransientDlg);
}

//----------------------------------------------------- Edit Book -----------------------------------------

void
EditBookProc ()
{
    EditBookEvent();
}

//--------------------------------------------------- New Shuffle Game ------------------------------

static void SetRandom P((int n));

static int
ShuffleOK (int n)
{
    ResetGameEvent();
    return 1;
}

static Option shuffleOptions[] = {
  {   0,  0,   50, NULL, (void*) &shuffleOpenings, NULL, NULL, CheckBox, N_("shuffle") },
  { 0,-1,2000000000, NULL, (void*) &appData.defaultFrcPosition, "", NULL, Spin, N_("Start-position number:") },
  {   0,  0,    0, NULL, (void*) &SetRandom, NULL, NULL, Button, N_("randomize") },
  {   0,  1,    0, NULL, (void*) &SetRandom, NULL, NULL, Button, N_("pick fixed") },
  {   0,  1,    0, NULL, (void*) &ShuffleOK, "", NULL, EndMark , "" }
};

static void
SetRandom (int n)
{
    int r = n==2 ? -1 : random() & (1<<30)-1;
    char buf[MSG_SIZ];
    snprintf(buf, MSG_SIZ,  "%d", r);
    SetWidgetText(&shuffleOptions[1], buf, TransientDlg);
    SetWidgetState(&shuffleOptions[0], True);
}

void
ShuffleMenuProc ()
{
    GenericPopUp(shuffleOptions, _("New Shuffle Game"), TransientDlg);
}

//------------------------------------------------------ Time Control -----------------------------------

static int TcOK P((int n));
int tmpMoves, tmpTc, tmpInc, tmpOdds1, tmpOdds2, tcType;

static void
ShowTC (int n)
{
}

static void SetTcType P((int n));

static char *
Value (int n)
{
	static char buf[MSG_SIZ];
	snprintf(buf, MSG_SIZ, "%d", n);
	return buf;
}

static Option tcOptions[] = {
{   0,  0,    0, NULL, (void*) &SetTcType, NULL, NULL, Button, N_("classical") },
{   0,  1,    0, NULL, (void*) &SetTcType, NULL, NULL, Button, N_("incremental") },
{   0,  1,    0, NULL, (void*) &SetTcType, NULL, NULL, Button, N_("fixed max") },
{   0,  0,  200, NULL, (void*) &tmpMoves, NULL, NULL, Spin, N_("Moves per session:") },
{   0,  0,10000, NULL, (void*) &tmpTc, NULL, NULL, Spin, N_("Initial time (min):") },
{   0, 0, 10000, NULL, (void*) &tmpInc, NULL, NULL, Spin, N_("Increment or max (sec/move):") },
{   0,  0,    0, NULL, NULL, NULL, NULL, Label, N_("Time-Odds factors:") },
{   0,  1, 1000, NULL, (void*) &tmpOdds1, NULL, NULL, Spin, N_("Engine #1") },
{   0,  1, 1000, NULL, (void*) &tmpOdds2, NULL, NULL, Spin, N_("Engine #2 / Human") },
{   0,  0,    0, NULL, (void*) &TcOK, "", NULL, EndMark , "" }
};

static int
TcOK (int n)
{
    char *tc;
    if(tcType == 0 && tmpMoves <= 0) return 0;
    if(tcType == 2 && tmpInc <= 0) return 0;
    GetWidgetText(&tcOptions[4], &tc); // get original text, in case it is min:sec
    searchTime = 0;
    switch(tcType) {
      case 0:
	if(!ParseTimeControl(tc, -1, tmpMoves)) return 0;
	appData.movesPerSession = tmpMoves;
	ASSIGN(appData.timeControl, tc);
	appData.timeIncrement = -1;
	break;
      case 1:
	if(!ParseTimeControl(tc, tmpInc, 0)) return 0;
	ASSIGN(appData.timeControl, tc);
	appData.timeIncrement = tmpInc;
	break;
      case 2:
	searchTime = tmpInc;
    }
    appData.firstTimeOdds = first.timeOdds = tmpOdds1;
    appData.secondTimeOdds = second.timeOdds = tmpOdds2;
    Reset(True, True);
    return 1;
}

static void
SetTcType (int n)
{
    switch(tcType = n) {
      case 0:
	SetWidgetText(&tcOptions[3], Value(tmpMoves), TransientDlg);
	SetWidgetText(&tcOptions[4], Value(tmpTc), TransientDlg);
	SetWidgetText(&tcOptions[5], _("Unused"), TransientDlg);
	break;
      case 1:
	SetWidgetText(&tcOptions[3], _("Unused"), TransientDlg);
	SetWidgetText(&tcOptions[4], Value(tmpTc), TransientDlg);
	SetWidgetText(&tcOptions[5], Value(tmpInc), TransientDlg);
	break;
      case 2:
	SetWidgetText(&tcOptions[3], _("Unused"), TransientDlg);
	SetWidgetText(&tcOptions[4], _("Unused"), TransientDlg);
	SetWidgetText(&tcOptions[5], Value(tmpInc), TransientDlg);
    }
}

void
TimeControlProc ()
{
   tmpMoves = appData.movesPerSession;
   tmpInc = appData.timeIncrement; if(tmpInc < 0) tmpInc = 0;
   tmpOdds1 = tmpOdds2 = 1; tcType = 0;
   tmpTc = atoi(appData.timeControl);
   GenericPopUp(tcOptions, _("Time Control"), TransientDlg);
}

//---------------------------- Chat Windows ----------------------------------------------

void
OutputChatMessage (int partner, char *mess)
{
    return; // dummy
}

