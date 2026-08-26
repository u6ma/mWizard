/* 
 * Motif
 *
 * Copyright (c) 1987-2012, The Open Group. All rights reserved.
 * Copyright (c) 2018-2026, alx@fastestcode.org
 *
 * Modified 2026 for mWizard, a fork of EMWM. See NOTICE for details.
 *
 * These libraries and programs are free software; you can
 * redistribute them and/or modify them under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * These libraries and programs are distributed in the hope that
 * they will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with these librararies and programs; if not, write
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110-1301 USA
*/ 

void ProcessWmFile (WmScreenData *pSD);
void ProcessCommandLine (int argc,  char *argv[]);
void ProcessMotifBindings (void);
void GetActionIndex (int tableSize, int *actionIndex);
void GetFunctionTableValues (int *execIndex, int *nopIndex, int *actionIndex);
void GetNopIndex (int tableSize, int *nopIndex);
void GetExecIndex (int tableSize, int *execIndex);
void            FreeMenuItem (MenuItem *menuItem);

unsigned char * GetNextLine (void);
unsigned char * GetString (unsigned char **linePP);
unsigned int PeekAhead(unsigned char *currentChar,
		       unsigned int currentLev);
Boolean ParseBtnEvent (unsigned char  **linePP,
                              unsigned int *eventType,
                              unsigned int *button,
                              unsigned int *state,
                              Boolean      *fClick);

void            ParseButtonStr (WmScreenData *pSD, unsigned char *buttonStr);
void            ParseKeyStr (WmScreenData *pSD, unsigned char *keyStr);
Boolean ParseKeyEvent (unsigned char **linePP, unsigned int *eventType,
		       KeyCode *keyCode, KeySym *pKeySym, unsigned int *state);
MenuItem      * ParseMwmMenuStr (WmScreenData *pSD, unsigned char *menuStr);
int             ParseWmFunction (unsigned char **linePP, unsigned int res_spec, WmFunction *pWmFunction);
void            PWarning (char *message);
void            SaveMenuAccelerators (WmScreenData *pSD, MenuSpec *newMenuSpec);
void      ScanAlphanumeric (unsigned char **linePP);
void            ScanWhitespace(unsigned char  **linePP);
void            ToLower (unsigned char  *string);
void		SyncModifierStrings(void);

#define GetSmartString(s)	GetString (s)

#include <stdio.h>
FILE *FopenConfigFile (void);
