/*
 * hide ~ A tool for hiding data inside images
 * Copyright © 2014-2026, albinoloverats ~ Software Development
 * email: hide@albinoloverats.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>

#include <dlfcn.h>
#include <pthread.h>

#include <sys/stat.h>

/* submodule includes */

#include "common.h"
#include "error.h"
#include "cli.h"
//#include "src/version.h"
#include "config.h"
#include "list.h"

/* project includes */

#include "hide.h"

#ifdef BUILD_GUI
	#include "gui-gtk.h"
#endif


static config_about_s about =
{
	"hide",
	HIDE_VERSION,
	PROJECT_URL,
	NULL
};


int main(int argc, char **argv)
{
	error_init();
#ifdef __DEBUG__
	cli_eprintf("\n" ANSI_COLOUR_RED "**** %s ****" ANSI_COLOUR_RESET "\n\n", _("DEBUG BUILD"));
#endif
#if 0
	/*
	 * (eventually) start background thread to check for newer version
	 */
	version_check_for_update(HIDE_VERSION, UPDATE_URL, DOWNLOAD_URL_TEMPLATE);
#endif

	list_t args = list_init(config_named_compare, false, false);

	list_add(args, &((config_named_s){ 'i', "input",             _("file"), _("The file to hide within the image"),          { CONFIG_ARG_REQ_STRING, { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'x', "image",             _("file"), _("The image file within which to hide/search"), { CONFIG_ARG_REQ_STRING, { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'o', "output",            _("file"), _("Where to put the output"),                    { CONFIG_ARG_REQ_STRING, { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 's', "supported-formats", NULL,      _("List supported image formats"),               { CONFIG_ARG_BOOLEAN,    { .string  = NULL  } }, false, false, false, false }));

	list_add(args, &((config_named_s){ 'f', "fill",              NULL,      _("Whether to fill the image data "),            { CONFIG_ARG_BOOLEAN,    { .boolean = false } }, false, true,  false, false }));

	list_t notes = list_default();
	list_add(notes, _("Will not clobber an image; you keep your originals original."));
	list_add(notes, _("Supplying an input file will result in an attempt to hide that file in the given image, and will produce a new image as the output. All three files are required."));
	list_add(notes, _("Not supplying an input file will result in an attempt to extract a hidden file and dump it in the output. Either input/image and output only."));

	config_init(about);
	config_parse(argc, argv, args, NULL, notes);
	list_deinit(notes);

	char *input  = ((config_named_s *)list_get(args, 0))->response.value.string;
	char *image  = ((config_named_s *)list_get(args, 1))->response.value.string;
	char *output = ((config_named_s *)list_get(args, 2))->response.value.string;
	bool formats = ((config_named_s *)list_get(args, 3))->response.value.boolean;
	bool fill    = ((config_named_s *)list_get(args, 4))->response.value.boolean;

	hide_action_e action = UNKNOWN;

	if (input && output && image)
		action = HIDE;
	else if (input && output && !image)
	{
		action = FIND;
		image = input;
	}
	else if (!input && output && image)
		action = FIND;

	/*
	 * single image provided; show information about that image
	 */
	if (action == UNKNOWN && image)
	{
		struct stat s;
		if (stat(image, &s) < 0 && errno == ENOENT)
			die(_("Could not read file %s\n"), image);

		image_info_s image_info = { image, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, NULL };
#ifndef __DEBUG_JPEG__
		void *so = find_supported_formats(DIR_LIBRARY, &image_info);
		if (!so)
			return errno;
#else
		extern uint64_t info_jpeg(image_info_s *image_info);
		extern void free_jpeg(image_info_s image_info);
		image_info.info = info_jpeg;
		image_info.free = free_jpeg;
#endif
		if (!image_info.info)
		{
			cli_eprintf("Unsupported image format\n");
			find_supported_formats(DIR_LIBRARY, NULL);
			errno = EFTYPE;
		}
		else
		{
			setlocale(LC_NUMERIC, "");
			cli_printf("File capacity: %'" PRIu64 " bytes\n", image_info.info(&image_info));
			image_info.free(image_info);
			errno = EXIT_SUCCESS;
		}
#ifndef __DEBUG_JPEG__
		dlclose(so);
#endif
		return errno;
	}
	else if (action == UNKNOWN || formats)
	{
		find_supported_formats(DIR_LIBRARY, NULL);
		return EXIT_FAILURE;
	}

	hide_files_s files = { image, action == HIDE ? input : output, action == HIDE ? output : NULL };
	process_options_s options = { NULL, files, action, fill, EXIT_FAILURE };

#ifndef __DEBUG__
	cli_status_e ui_status = CLI_INIT;
	cli_progress_s ui_current = { 0, 1, NULL }; /* updated after reading image */
	cli_progress_s ui_total   = { 0, 3, NULL }; /* maximum of 3 steps (read, update, write) */
	cli_s ui = { 0x0 };
	options.ui = &ui;
	ui.status = &ui_status;
	ui.current = &ui_current;
	ui.total = &ui_total;
	/*
	 * TODO start process() in own thread, then call cli_display()
	 *
	 * remember that process() will have to set the status once it's finished
	 */
	pthread_t t;
	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_JOINABLE);
	int error = pthread_create(&t, &a, process, &options);
	pthread_attr_destroy(&a);
	if (error)
		die(_("Could not create worker thread: %s\n"), strerror(error));

	cli_display(&ui);

	error = pthread_join(t, NULL);
	if (error)
		die(_("Could not join worker thread: %s\n"), strerror(error));
#else
	process(&options);
#endif

	return options.result;
}
