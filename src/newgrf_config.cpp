/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_config.cpp Finding NewGRFs and configuring them. */

#include "stdafx.h"
#include "debug.h"
#include "3rdparty/md5/md5.h"
#include "newgrf.h"
#include "gamelog.h"
#include "network/network_func.h"
#include "gfx_func.h"

#include "fileio_func.h"
#include "fios.h"

GRFConfig::GRFConfig(const char *filename)
{
	if (filename != NULL) this->filename = strdup(filename);
}

GRFConfig::~GRFConfig()
{
	/* GCF_COPY as in NOT strdupped/alloced the filename, name and info */
	if (!HasBit(this->flags, GCF_COPY)) {
		free(this->filename);
		free(this->name);
		free(this->info);
		delete this->error;
	}
}

/**
 * Get the name of this grf. In case the name isn't known
 * the filename is returned.
 * @return The name of filename of this grf.
 */
const char *GRFConfig::GetName() const
{
	if (StrEmpty(this->name)) return this->filename;
	return this->name;
}

/**
 * Get the grf info.
 * @return A string with a description of this grf.
 */
const char *GRFConfig::GetDescription() const
{
	return this->info;
}

GRFConfig *_all_grfs;
GRFConfig *_grfconfig;
GRFConfig *_grfconfig_newgame;
GRFConfig *_grfconfig_static;

GRFError::GRFError(StringID severity, StringID message) :
	message(message),
	severity(severity)
{
}

GRFError::~GRFError()
{
	free(this->custom_message);
	free(this->data);
}

/**
 * Update the palettes of the graphics from the config file.
 * This is needed because the config file gets read and parsed
 * before the palette is chosen (one can configure the base
 * graphics set governing the palette in the config after all).
 * As a result of this we update the settings from the config
 * once we have determined the palette.
 */
void UpdateNewGRFConfigPalette()
{
	for (GRFConfig *c = _grfconfig_newgame; c != NULL; c = c->next) c->windows_paletted = (_use_palette == PAL_WINDOWS);
	for (GRFConfig *c = _grfconfig_static;  c != NULL; c = c->next) c->windows_paletted = (_use_palette == PAL_WINDOWS);
}

/** Calculate the MD5 sum for a GRF, and store it in the config.
 * @param config GRF to compute.
 * @return MD5 sum was successfully computed
 */
static bool CalcGRFMD5Sum(GRFConfig *config)
{
	FILE *f;
	Md5 checksum;
	uint8 buffer[1024];
	size_t len, size;

	/* open the file */
	f = FioFOpenFile(config->filename, "rb", DATA_DIR, &size);
	if (f == NULL) return false;

	/* calculate md5sum */
	while ((len = fread(buffer, 1, (size > sizeof(buffer)) ? sizeof(buffer) : size, f)) != 0 && size != 0) {
		size -= len;
		checksum.Append(buffer, len);
	}
	checksum.Finish(config->ident.md5sum);

	FioFCloseFile(f);

	return true;
}


/** Find the GRFID of a given grf, and calculate its md5sum.
 * @param config    grf to fill.
 * @param is_static grf is static.
 * @return Operation was successfully completed.
 */
bool FillGRFDetails(GRFConfig *config, bool is_static)
{
	if (!FioCheckFileExists(config->filename)) {
		config->status = GCS_NOT_FOUND;
		return false;
	}

	/* Find and load the Action 8 information */
	LoadNewGRFFile(config, CONFIG_SLOT, GLS_FILESCAN);

	/* Skip if the grfid is 0 (not read) or 0xFFFFFFFF (ttdp system grf) */
	if (config->ident.grfid == 0 || config->ident.grfid == 0xFFFFFFFF || config->IsOpenTTDBaseGRF()) return false;

	if (is_static) {
		/* Perform a 'safety scan' for static GRFs */
		LoadNewGRFFile(config, 62, GLS_SAFETYSCAN);

		/* GCF_UNSAFE is set if GLS_SAFETYSCAN finds unsafe actions */
		if (HasBit(config->flags, GCF_UNSAFE)) return false;
	}

	config->windows_paletted = (_use_palette == PAL_WINDOWS);

	return CalcGRFMD5Sum(config);
}


/** Clear a GRF Config list, freeing all nodes.
 * @param config Start of the list.
 * @post \a config is set to \c NULL.
 */
void ClearGRFConfigList(GRFConfig **config)
{
	GRFConfig *c, *next;
	for (c = *config; c != NULL; c = next) {
		next = c->next;
		delete c;
	}
	*config = NULL;
}


/**
 * Make a deep copy of a GRFConfig.
 * @param c the grfconfig to copy
 * @return A pointer to a new grfconfig that's a copy of the original
 */
GRFConfig *DuplicateGRFConfig(const GRFConfig *c)
{
	GRFConfig *config = new GRFConfig();
	*config = *c;

	if (c->filename != NULL) config->filename = strdup(c->filename);
	if (c->name     != NULL) config->name = strdup(c->name);
	if (c->info     != NULL) config->info = strdup(c->info);
	if (c->error    != NULL) {
		config->error = new GRFError(c->error->severity, c->error->message);
		config->error->num_params = c->error->num_params;
		memcpy(config->error->param_value, c->error->param_value, sizeof(config->error->param_value));
		if (c->error->data           != NULL) config->error->data = strdup(c->error->data);
		if (c->error->custom_message != NULL) config->error->custom_message = strdup(c->error->custom_message);
	}

	ClrBit(config->flags, GCF_COPY);

	return config;
}

/** Copy a GRF Config list
 * @param dst pointer to destination list
 * @param src pointer to source list values
 * @param init_only the copied GRF will be processed up to GLS_INIT
 * @return pointer to the last value added to the destination list */
GRFConfig **CopyGRFConfigList(GRFConfig **dst, const GRFConfig *src, bool init_only)
{
	/* Clear destination as it will be overwritten */
	ClearGRFConfigList(dst);
	for (; src != NULL; src = src->next) {
		GRFConfig *c = DuplicateGRFConfig(src);

		ClrBit(c->flags, GCF_INIT_ONLY);
		if (init_only) SetBit(c->flags, GCF_INIT_ONLY);

		*dst = c;
		dst = &c->next;
	}

	return dst;
}

/**
 * Removes duplicates from lists of GRFConfigs. These duplicates
 * are introduced when the _grfconfig_static GRFs are appended
 * to the _grfconfig on a newgame or savegame. As the parameters
 * of the static GRFs could be different that the parameters of
 * the ones used non-statically. This can result in desyncs in
 * multiplayers, so the duplicate static GRFs have to be removed.
 *
 * This function _assumes_ that all static GRFs are placed after
 * the non-static GRFs.
 *
 * @param list the list to remove the duplicates from
 */
static void RemoveDuplicatesFromGRFConfigList(GRFConfig *list)
{
	GRFConfig *prev;
	GRFConfig *cur;

	if (list == NULL) return;

	for (prev = list, cur = list->next; cur != NULL; prev = cur, cur = cur->next) {
		if (cur->ident.grfid != list->ident.grfid) continue;

		prev->next = cur->next;
		delete cur;
		cur = prev; // Just go back one so it continues as normal later on
	}

	RemoveDuplicatesFromGRFConfigList(list->next);
}

/**
 * Appends the static GRFs to a list of GRFs
 * @param dst the head of the list to add to
 */
void AppendStaticGRFConfigs(GRFConfig **dst)
{
	GRFConfig **tail = dst;
	while (*tail != NULL) tail = &(*tail)->next;

	CopyGRFConfigList(tail, _grfconfig_static, false);
	RemoveDuplicatesFromGRFConfigList(*dst);
}

/** Appends an element to a list of GRFs
 * @param dst the head of the list to add to
 * @param el the new tail to be */
void AppendToGRFConfigList(GRFConfig **dst, GRFConfig *el)
{
	GRFConfig **tail = dst;
	while (*tail != NULL) tail = &(*tail)->next;
	*tail = el;

	RemoveDuplicatesFromGRFConfigList(*dst);
}


/** Reset the current GRF Config to either blank or newgame settings. */
void ResetGRFConfig(bool defaults)
{
	CopyGRFConfigList(&_grfconfig, _grfconfig_newgame, !defaults);
	AppendStaticGRFConfigs(&_grfconfig);
}


/** Check if all GRFs in the GRF config from a savegame can be loaded.
 * @return will return any of the following 3 values:<br>
 * <ul>
 * <li> GLC_ALL_GOOD: No problems occured, all GRF files were found and loaded
 * <li> GLC_COMPATIBLE: For one or more GRF's no exact match was found, but a
 *     compatible GRF with the same grfid was found and used instead
 * <li> GLC_NOT_FOUND: For one or more GRF's no match was found at all
 * </ul> */
GRFListCompatibility IsGoodGRFConfigList()
{
	GRFListCompatibility res = GLC_ALL_GOOD;

	for (GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		const GRFConfig *f = FindGRFConfig(c->ident.grfid, c->ident.md5sum);
		if (f == NULL) {
			char buf[256];

			/* If we have not found the exactly matching GRF try to find one with the
			 * same grfid, as it most likely is compatible */
			f = FindGRFConfig(c->ident.grfid);
			if (f != NULL) {
				md5sumToString(buf, lastof(buf), c->ident.md5sum);
				DEBUG(grf, 1, "NewGRF %08X (%s) not found; checksum %s. Compatibility mode on", BSWAP32(c->ident.grfid), c->filename, buf);
				SetBit(c->flags, GCF_COMPATIBLE);

				/* Non-found has precedence over compatibility load */
				if (res != GLC_NOT_FOUND) res = GLC_COMPATIBLE;
				GamelogGRFCompatible(&f->ident);
				goto compatible_grf;
			}

			/* No compatible grf was found, mark it as disabled */
			md5sumToString(buf, lastof(buf), c->ident.md5sum);
			DEBUG(grf, 0, "NewGRF %08X (%s) not found; checksum %s", BSWAP32(c->ident.grfid), c->filename, buf);

			GamelogGRFRemove(c->ident.grfid);

			c->status = GCS_NOT_FOUND;
			res = GLC_NOT_FOUND;
		} else {
compatible_grf:
			DEBUG(grf, 1, "Loading GRF %08X from %s", BSWAP32(f->ident.grfid), f->filename);
			/* The filename could be the filename as in the savegame. As we need
			 * to load the GRF here, we need the correct filename, so overwrite that
			 * in any case and set the name and info when it is not set already.
			 * When the GCF_COPY flag is set, it is certain that the filename is
			 * already a local one, so there is no need to replace it. */
			if (!HasBit(c->flags, GCF_COPY)) {
				free(c->filename);
				c->filename = strdup(f->filename);
				memcpy(c->ident.md5sum, f->ident.md5sum, sizeof(c->ident.md5sum));
				if (c->name == NULL && f->name != NULL) c->name = strdup(f->name);
				if (c->info == NULL && f->info != NULL) c->info = strdup(f->info);
				c->error = NULL;
			}
		}
	}

	return res;
}

/** Helper for scanning for files with GRF as extension */
class GRFFileScanner : FileScanner {
public:
	/* virtual */ bool AddFile(const char *filename, size_t basepath_length);

	/** Do the scan for GRFs. */
	static uint DoScan()
	{
		GRFFileScanner fs;
		return fs.Scan(".grf", DATA_DIR);
	}
};

bool GRFFileScanner::AddFile(const char *filename, size_t basepath_length)
{
	GRFConfig *c = new GRFConfig(filename + basepath_length);

	bool added = true;
	if (FillGRFDetails(c, false)) {
		if (_all_grfs == NULL) {
			_all_grfs = c;
		} else {
			/* Insert file into list at a position determined by its
			 * name, so the list is sorted as we go along */
			GRFConfig **pd, *d;
			bool stop = false;
			for (pd = &_all_grfs; (d = *pd) != NULL; pd = &d->next) {
				if (c->ident.grfid == d->ident.grfid && memcmp(c->ident.md5sum, d->ident.md5sum, sizeof(c->ident.md5sum)) == 0) added = false;
				/* Because there can be multiple grfs with the same name, make sure we checked all grfs with the same name,
				 *  before inserting the entry. So insert a new grf at the end of all grfs with the same name, instead of
				 *  just after the first with the same name. Avoids doubles in the list. */
				if (strcasecmp(c->GetName(), d->GetName()) <= 0) {
					stop = true;
				} else if (stop) {
					break;
				}
			}
			if (added) {
				c->next = d;
				*pd = c;
			}
		}
	} else {
		added = false;
	}

	if (!added) {
		/* File couldn't be opened, or is either not a NewGRF or is a
		 * 'system' NewGRF or it's already known, so forget about it. */
		delete c;
	}

	return added;
}

/**
 * Simple sorter for GRFS
 * @param p1 the first GRFConfig *
 * @param p2 the second GRFConfig *
 * @return the same strcmp would return for the name of the NewGRF.
 */
static int CDECL GRFSorter(GRFConfig * const *p1, GRFConfig * const *p2)
{
	const GRFConfig *c1 = *p1;
	const GRFConfig *c2 = *p2;

	return strcasecmp(c1->GetName(), c2->GetName());
}

/** Scan for all NewGRFs. */
void ScanNewGRFFiles()
{
	ClearGRFConfigList(&_all_grfs);

	DEBUG(grf, 1, "Scanning for NewGRFs");
	uint num = GRFFileScanner::DoScan();

	DEBUG(grf, 1, "Scan complete, found %d files", num);
	if (num == 0 || _all_grfs == NULL) return;

	/* Sort the linked list using quicksort.
	 * For that we first have to make an array, then sort and
	 * then remake the linked list. */
	GRFConfig **to_sort = MallocT<GRFConfig*>(num);

	uint i = 0;
	for (GRFConfig *p = _all_grfs; p != NULL; p = p->next, i++) {
		to_sort[i] = p;
	}
	/* Number of files is not necessarily right */
	num = i;

	QSortT(to_sort, num, &GRFSorter);

	for (i = 1; i < num; i++) {
		to_sort[i - 1]->next = to_sort[i];
	}
	to_sort[num - 1]->next = NULL;
	_all_grfs = to_sort[0];

	free(to_sort);

#ifdef ENABLE_NETWORK
	NetworkAfterNewGRFScan();
#endif
}


/** Find a NewGRF in the scanned list.
 * @param grfid GRFID to look for,
 * @param md5sum Expected MD5 sum (set to \c NULL if not relevant).
 * @return The matching grf, if it exists in #_all_grfs, else \c NULL.
 */
const GRFConfig *FindGRFConfig(uint32 grfid, const uint8 *md5sum)
{
	for (const GRFConfig *c = _all_grfs; c != NULL; c = c->next) {
		if (c->ident.grfid == grfid) {
			if (md5sum == NULL) return c;

			if (memcmp(md5sum, c->ident.md5sum, sizeof(c->ident.md5sum)) == 0) return c;
		}
	}

	return NULL;
}

#ifdef ENABLE_NETWORK

/** Structure for UnknownGRFs; this is a lightweight variant of GRFConfig */
struct UnknownGRF : public GRFIdentifier {
	UnknownGRF *next;
	char   name[NETWORK_GRF_NAME_LENGTH];
};

/**
 * Finds the name of a NewGRF in the list of names for unknown GRFs. An
 * unknown GRF is a GRF where the .grf is not found during scanning.
 *
 * The names are resolved via UDP calls to servers that should know the name,
 * though the replies may not come. This leaves "<Unknown>" as name, though
 * that shouldn't matter _very_ much as they need GRF crawler or so to look
 * up the GRF anyway and that works better with the GRF ID.
 *
 * @param grfid  the GRF ID part of the 'unique' GRF identifier
 * @param md5sum the MD5 checksum part of the 'unique' GRF identifier
 * @param create whether to create a new GRFConfig if the GRFConfig did not
 *               exist in the fake list of GRFConfigs.
 * @return the GRFConfig with the given GRF ID and MD5 checksum or NULL when
 *         it does not exist and create is false. This value must NEVER be
 *         freed by the caller.
 */
char *FindUnknownGRFName(uint32 grfid, uint8 *md5sum, bool create)
{
	UnknownGRF *grf;
	static UnknownGRF *unknown_grfs = NULL;

	for (grf = unknown_grfs; grf != NULL; grf = grf->next) {
		if (grf->grfid == grfid) {
			if (memcmp(md5sum, grf->md5sum, sizeof(grf->md5sum)) == 0) return grf->name;
		}
	}

	if (!create) return NULL;

	grf = CallocT<UnknownGRF>(1);
	grf->grfid = grfid;
	grf->next  = unknown_grfs;
	strecpy(grf->name, UNKNOWN_GRF_NAME_PLACEHOLDER, lastof(grf->name));
	memcpy(grf->md5sum, md5sum, sizeof(grf->md5sum));

	unknown_grfs = grf;
	return grf->name;
}

#endif /* ENABLE_NETWORK */


/** Retrieve a NewGRF from the current config by its grfid.
 * @param grfid grf to look for.
 * @param mask  GRFID mask to allow for partial matching.
 * @return The grf config, if it exists, else \c NULL.
 */
GRFConfig *GetGRFConfig(uint32 grfid, uint32 mask)
{
	GRFConfig *c;

	for (c = _grfconfig; c != NULL; c = c->next) {
		if ((c->ident.grfid & mask) == (grfid & mask)) return c;
	}

	return NULL;
}


/** Build a string containing space separated parameter values, and terminate */
char *GRFBuildParamList(char *dst, const GRFConfig *c, const char *last)
{
	uint i;

	/* Return an empty string if there are no parameters */
	if (c->num_params == 0) return strecpy(dst, "", last);

	for (i = 0; i < c->num_params; i++) {
		if (i > 0) dst = strecpy(dst, " ", last);
		dst += seprintf(dst, last, "%d", c->param[i]);
	}
	return dst;
}

/** Base GRF ID for OpenTTD's base graphics GRFs. */
static const uint32 OPENTTD_GRAPHICS_BASE_GRF_ID = BSWAP32(0xFF4F5400);

/**
 * Checks whether this GRF is a OpenTTD base graphic GRF.
 * @return true if and only if it is a base GRF.
 */
bool GRFConfig::IsOpenTTDBaseGRF() const
{
	return (this->ident.grfid & 0x00FFFFFF) == OPENTTD_GRAPHICS_BASE_GRF_ID;
}
