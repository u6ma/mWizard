/* 
 * Motif
 *
 * Copyright (c) 1987-2012, The Open Group. All rights reserved.
 * Copyright (c) 2018-2026, alx@fastestcode.org
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

void ConfirmAction (WmScreenData *pSD, int nbr);
void HideFeedbackWindow (WmScreenData *pSD);

/*
 * A single styled line in the same feedback box, taken away after a timeout.
 * See the note above ShowTextFeedback() for why the workspace notice reuses
 * this window rather than putting up one of its own.
 */
void ShowTextFeedback (WmScreenData *pSD, int monitor, const char *text,
	int timeout);

/*
 * The optional notice on a workspace switch: the feedback box, a command, or
 * nothing, according to the workspaceFeedback resource.
 */
void AnnounceWorkspace (WmScreenData *pSD, WmWorkspaceData *pWS, int monitor);
void InitCursorInfo (void);
void PaintFeedbackWindow (WmScreenData *pSD);
void ShowFeedbackWindow (WmScreenData *pSD, int x, int y, 
				unsigned int width, unsigned int height, 
				unsigned long style);
void ShowWaitState (Boolean flag);
void UpdateFeedbackInfo (WmScreenData *pSD, int x, int y, 
				unsigned int width, unsigned int height);
void UpdateFeedbackText (WmScreenData *pSD, int x, int y, 
				unsigned int width, unsigned int height);

