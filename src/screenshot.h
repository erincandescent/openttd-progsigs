/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file screenshot.h Functions to make screenshots. */

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

void InitializeScreenshotFormats();

const char *GetScreenshotFormatDesc(int i);
void SetScreenshotFormat(int i);

/** Type of requested screenshot */
enum ScreenshotType {
	SC_VIEWPORT, ///< Screenshot of viewport
	SC_RAW,      ///< Raw screenshot from blitter buffer
	SC_WORLD,    ///< World screenshot
};

bool MakeScreenshot(ScreenshotType t, const char *name);

extern char _screenshot_format_name[8];
extern uint _num_screenshot_formats;
extern uint _cur_screenshot_format;
extern char _full_screenshot_name[MAX_PATH];

#endif /* SCREENSHOT_H */
