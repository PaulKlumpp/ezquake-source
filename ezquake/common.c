/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: common.c,v 1.21 2006-04-29 11:38:51 johnnycz Exp $
*/

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifdef SERVERONLY 
#include "qwsvdef.h"
#else
#include "quakedef.h"
#endif

void Draw_BeginDisc ();
void Draw_EndDisc ();

#define MAX_NUM_ARGVS	50

usercmd_t nullcmd; // guaranteed to be zero

static char	*largv[MAX_NUM_ARGVS + 1];

cvar_t	developer = {"developer", "0"};
cvar_t	registered = {"registered", "0"};
cvar_t	mapname = {"mapname", "", CVAR_ROM};

qbool com_serveractive = false;

qbool com_filefrompak;
char *com_filesearchpath;

char *com_args_original;

void FS_InitFilesystem (void);
void COM_Path_f (void);

char com_gamedirfile[MAX_QPATH];

int Q_strlen (char *str)
{
    int     count;
    
    count = 0;
    while (str[count])
        count++;

    return count;
}

/*
All of Quake's data access is through a hierarchic file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.
*/

//============================================================================

void COM_StoreOriginalCmdline(int argc, char **argv)
{
    char buf[4096]; // enough?
    int i;
    qbool first = true;

    buf[0] = 0;
    for (i=0; i < argc; i++)
    {
        if (!first)
            strcat(buf, " ");
        first = false;

        strcat(buf, argv[i]);
    }

    com_args_original = Q_strdup(buf);
}

// ClearLink is used for new headnodes
void ClearLink (link_t *l) {
	l->prev = l->next = l;
}

void RemoveLink (link_t *l) {
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before) {
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter (link_t *l, link_t *after) {
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================
					LIBRARY REPLACEMENT FUNCTIONS
============================================================================
*/

// same as strcpy but
// returns pointer to the end of dest string
char* Q_strcpy(char* dest, const char* src)
{
	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;

	return dest;
}

// same as strcat but
// returns pointer to the end of dest string
char* Q_strcat(char* dest, const char* src)
{
	while (*dest)
		dest++;
	
	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;

	return dest;
}

int Q_atoi (const char *str) {
	int val, sign, c;
	
	if (*str == '-') {
		sign = -1;
		str++;
	} else {
		sign = 1;
	}
		
	val = 0;

	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') ) {
		str += 2;
		while (1) {
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	// check for character
	if (str[0] == '\'')
		return sign * str[1];

	// assume decimal
	while (1) {
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val * 10 + c - '0';
	}

	return 0;
}

float Q_atof (const char *str) {
	double val;
	int sign, c, decimal, total;
	
	if (*str == '-') {
		sign = -1;
		str++;
	} else {
		sign = 1;
	}
		
	val = 0;

	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') ) {
		str += 2;
		while (1) {
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	// check for character
	if (str[0] == '\'')
		return sign * str[1];

	// assume decimal
	decimal = -1;
	total = 0;
	while (1) {
		c = *str++;
		if (c == '.') {
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
			break;
		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val * sign;
	while (total > decimal) {
		val /= 10;
		total--;
	}

	return val * sign;
}

// removes trailing zeros
char *Q_ftos (float value)
{
	static char str[128];
	int	i;

	snprintf (str, sizeof(str), "%f", value);

	for (i=strlen(str)-1 ; i>0 && str[i]=='0' ; i--)
		str[i] = 0;
	if (str[i] == '.')
		str[i] = 0;

	return str;
}

int Com_HashKey (const char *name) { 
	unsigned int v;	
	unsigned char c; 

	v = 0; 
	while ( (c = *name++) != 0 ) 
		v += c &~ 32;   // make it case insensitive 

	return v % 32; 
} 

// Added by VVD {
#ifdef _WIN32
int snprintf(char *buffer, size_t count, char const *format, ...)
{
	int ret;
	va_list argptr;
	if (!count) return 0;
	va_start(argptr, format);
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	va_end(argptr);
	return ret;
}
int vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	int ret;
	if (!count) return 0;
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	return ret;
}
#endif

#if defined(__linux__) || defined(_WIN32)
size_t strlcpy(char *dst, char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

		/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

size_t strlcat(char *dst, char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));       /* count does not include NUL */
}
char *strnstr(char *s, char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0' || slen-- < 1)
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif
// Added by VVD }

//============================================================================

/*
** Q_malloc
**
** Use it instead of malloc so that if memory allocation fails,
** the program exits with a message saying there's not enough memory
** instead of crashing after trying to use a NULL pointer
*/
void *Q_malloc (size_t size)
{
	void *p = malloc(size);
	if (!p)
		Sys_Error ("Not enough memory free; check disk space");

	memset(p, 0, size);

	return p;
}

void *Q_calloc (size_t n, size_t size) {
	void *p;

	if (!(p = calloc(n, size)))
		Sys_Error ("Not enough memory free; check disk space");
	return p;
}

char *Q_strdup (const char *src)
{
	char *p = strdup(src);
	if (!p)
		Sys_Error ("Not enough memory free; check disk space");
	return p;
}


/*
============================================================================
					BYTE ORDER FUNCTIONS
============================================================================
*/

short ShortSwap (short l) {
	byte b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

int LongSwap (int l) {
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

float FloatSwap (float f) {
	union {
		float	f;
		byte	b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

//===========================================================================

void SZ_Init (sizebuf_t *buf, byte *data, int length) {
	memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear (sizebuf_t *buf) {
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace (sizebuf_t *buf, int length) {
	void *data;

	if (buf->cursize + length > buf->maxsize) {
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%i/%i/%i)", buf->cursize, length, buf->maxsize);

		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i/%i is > full buffer size", length, buf->maxsize);

		Sys_Printf ("SZ_GetSpace: overflow\n");	// because Com_Printf may be redirected
		SZ_Clear (buf); 
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write (sizebuf_t *buf, void *data, int length) {
	memcpy (SZ_GetSpace(buf,length),data,length);		
}

void SZ_Print (sizebuf_t *buf, char *data) {
	int len;

	len = strlen(data) + 1;

	if (!buf->cursize || buf->data[buf->cursize-1])
		memcpy ((byte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
		memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
}

//============================================================================

char *COM_SkipPath (char *pathname) {
	char	*last;
	
	last = pathname;
	while (*pathname) {
		if (*pathname == '/')
			last = pathname+1;
		pathname++;
	}
	return last;
}


void COM_StripExtension (char *in, char *out) {
	char *dot;

	if (!(dot = strrchr(in, '.'))) {
		strlcpy(out, in, strlen(in) + 1);
		return;
	}
	while (*in && in != dot)
		*out++ = *in++;
	*out = 0;
}


char *COM_FileExtension (char *in) {
	static char exten[8];
	int i;

	if (!(in = strrchr(in, '.')))
		return "";
	in++;
	for (i = 0 ; i < 7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

//Extract file name without extension, to be used for hunk tags (up to 32 characters, including trailing zero)



void COM_FileBase (char *in, char *out) {
	char *start, *end;
	int	length;

	if (!(end = strrchr(in, '.')))
		end = in + strlen(in);

	if (!(start = strrchr(in, '/')))
		start = in;
	else
		start += 1;

	length = end - start;
	if (length < 0)	
		strcpy (out, "?empty filename?");	
	else if (length == 0)
		out[0] = 0;
	else {
		if (length > 31)
			length = 31;
		memcpy (out, start, length);
		out[length] = 0;
	}
}

//If path doesn't have a .EXT, append extension (extension should include the .)
void COM_DefaultExtension (char *path, char *extension) {
	char *src;

	src = path + strlen(path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strncat (path, extension, MAX_OSPATH);
}

//If path doesn't have an extension or has a different extension, append(!) specified extension
//Extension should include the .
void COM_ForceExtension (char *path, char *extension) {
	char *src;

	src = path + strlen(path) - strlen(extension);
	if (src >= path && !strcmp(src, extension))
		return;

	strncat (path, extension, MAX_OSPATH);
}

//============================================================================

#define MAX_COM_TOKEN	1024

char	com_token[MAX_COM_TOKEN];
int		com_argc;
char	**com_argv;

//Parse a token out of a string
char *COM_Parse (char *data) {
	unsigned char c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
while (true)
{
	while ( (c = *data) == ' ' || c == '\t' || c == '\r' || c == '\n')
		data++;

	if (c == 0)
		return NULL;			// end of file;

	// skip // comments
	if (c == '/' && data[1] == '/')
		while (*data && *data != '\n')
			data++;
	else
		break;
}

	// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || !c) {
				com_token[len] = 0;
				if (!c)
					data--;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		if (len >= MAX_COM_TOKEN - 1)
			break;
		c = *data;
	} while (c && c != ' ' && c != '\t' && c != '\n' && c != '\r');

	com_token[len] = 0;
	return data;
}

void COM_CheckRegistered (void) {
	FILE *h;

	FS_FOpenFile ("gfx/pop.lmp", &h);

	if (h) {
		Cvar_Set (&registered, "1");
		fclose (h);
	}
}

//Adds the given string at the end of the current argument list
void COM_AddParm (char *parm) {
	if (com_argc >= MAX_NUM_ARGVS)
		return;
	largv[com_argc++] = parm;
}

//Returns the position (1 to argc-1) in the program's argument list
//where the given parameter appears, or 0 if not present
int COM_CheckParm (char *parm) {
	int		i;

	for (i = 1; i < com_argc; i++) {
		if (!strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

int COM_Argc (void) {
	return com_argc;
}

char *COM_Argv (int arg) {
	if (arg < 0 || arg >= com_argc)
		return "";
	return com_argv[arg];
}

void COM_ClearArgv (int arg) {
	if (arg < 0 || arg >= com_argc)
		return;
	com_argv[arg] = "";
}

void COM_InitArgv (int argc, char **argv) {
	for (com_argc = 0; com_argc < MAX_NUM_ARGVS && com_argc < argc; com_argc++) {
		if (argv[com_argc])
			largv[com_argc] = argv[com_argc];
		else
			largv[com_argc] = "";
	}

	largv[com_argc] = "";
	com_argv = largv;
}

void COM_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register (&developer);
	Cvar_Register (&registered);
	Cvar_Register (&mapname);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("path", COM_Path_f);
}

void COM_Shutdown (void) {}

//does a varargs printf into a temp buffer, so I don't need to have varargs versions of all text functions.
char *va (char *format, ...) {
	va_list argptr;
	static char string[8][2048];
	static int idx = 0;

	idx++;
	if (idx == 8)
		idx = 0;

	va_start (argptr, format);
	vsnprintf (string[idx], sizeof(string[idx]), format, argptr);
	va_end (argptr);

	return string[idx];
}

char *CopyString (const char *in) {
	char *out;

	out = (char *) Z_Malloc (strlen(in) + 1);
	strcpy (out, in);
	return out;
}

/*
=============================================================================
QUAKE FILESYSTEM
=============================================================================
*/

int		com_filesize;
char	com_netpath[MAX_OSPATH];	

// in memory
typedef struct {
	char	name[MAX_QPATH];
	int		filepos, filelen;
} packfile_t;

typedef struct pack_s {
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

// on disk
typedef struct {
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct {
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

#define	MAX_FILES_IN_PACK	2048

char	com_gamedir[MAX_OSPATH];
char	com_basedir[MAX_OSPATH];

// QW262 -->
#ifndef SERVERONLY
char	userdirfile[MAX_OSPATH];
char	com_userdir[MAX_OSPATH];
int	userdir_type;
#endif
// <-- QW262

typedef struct searchpath_s {
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*com_searchpaths;
searchpath_t	*com_base_searchpaths;	// without gamedirs

/*
================
COM_FileLength
================
*/
int COM_FileLength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int COM_FileOpenRead (char *path, FILE **hndl) {
	FILE *f;

	if (!(f = fopen(path, "rb"))) {
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return COM_FileLength(f);
}

void COM_Path_f (void) {
	searchpath_t *s;

	Com_Printf ("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next) {
		if (s == com_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}
}

int COM_FCreateFile (char *filename, FILE **file, char *path, char *mode)
{
    searchpath_t *search;
    char fullpath[MAX_QPATH];

    if (path == NULL)
        path = com_gamedir;
    else
    {
        // check if given path is in one of mounted filesystem
        // we do not allow others
        for (search = com_searchpaths ; search ; search = search->next)
        {
            if (search->pack != NULL)
                continue;   // no writes to pak files

            if (strlen(search->filename) > strlen(path)  &&
                !strcmp(search->filename + strlen(search->filename) - strlen(path), path) &&
                *(search->filename + strlen(search->filename) - strlen(path) - 1) == '/')
            {
                break;
            }
        }
        if (search == NULL)
            Sys_Error("COM_FCreateFile: out of Quake filesystem");
    }

    if (mode == NULL)
        mode = "wb";

    // try to create
    sprintf(fullpath, "%s/%s/%s", com_basedir, path, filename);
    COM_CreatePath(fullpath);
    *file = fopen(fullpath, mode);

    if (*file == NULL)
    {
        // no Sys_Error, quake can be run from read-only media like cd-rom
        return 0;
    }

    Sys_Printf ("FCreateFile: %s", filename);

    return 1;
}

//The filename will be prefixed by com_basedir
qbool COM_WriteFile (char *filename, void *data, int len) {
	FILE *f;
	char name[MAX_OSPATH];

	snprintf (name, sizeof(name), "%s/%s", com_basedir, filename);

	if (!(f = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(f = fopen (name, "wb")))
			return false;
	}
	Sys_Printf ("COM_WriteFile: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);
	return true;
}

//Only used for CopyFile and download


void COM_CreatePath(char *path) {
	char *s, save;

	if (!*path)
		return;

	for (s = path + 1; *s; s++) {
#ifdef _WIN32
		if (*s == '/' || *s == '\\') {
#else
		if (*s == '/') {
#endif
			save = *s;
			*s = 0;
			Sys_mkdir(path);
			*s = save;
		}
	}
}

//Copies a file over from the net to the local cache, creating any directories
//needed.  This is for the convenience of developers using ISDN from home.
void COM_CopyFile (char *netpath, char *cachepath) {
	FILE *in, *out;
	int remaining, count;
	char buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);		
	COM_CreatePath (cachepath);	// create directories up to the cache file
	out = fopen(cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);

	while (remaining) {
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}

int FS_FOpenPathFile (char *filename, FILE **file) {

	*file = NULL;
	com_filesize = -1;
	com_netpath[0] = 0;

	if ((*file = fopen (filename, "rb"))) {

		if (developer.value)
			Sys_Printf ("FindFile: %s\n", com_netpath);

		com_filesize = COM_FileLength (*file);
		return com_filesize;

	}

	if (developer.value)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	return -1;
}

//Finds the file in the search path.
//Sets com_filesize, com_netpath and one of handle or file
qbool	file_from_pak;		// global indicating file came from a packfile
qbool	file_from_gamedir;	// global indicating file came from a gamedir (and gamedir wasn't id1/qw)

int FS_FOpenFile (char *filename, FILE **file) {
	searchpath_t *search;
	pack_t *pak;
	int i;

	*file = NULL;
	file_from_pak = false;
	file_from_gamedir = true;
	com_filesize = -1;
	com_netpath[0] = 0;

	// search through the path, one element at a time
	for (search = com_searchpaths; search; search = search->next) {
		if (search == com_base_searchpaths && com_searchpaths != com_base_searchpaths)
			file_from_gamedir = 0;

		// is the element a pak file?
		if (search->pack) {
			// look through all the pak file elements
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++) {
				if (!strcmp (pak->files[i].name, filename)) {	// found it!
					if (developer.value)
						Sys_Printf ("PackFile: %s : %s\n", pak->filename, filename);
					// open a new file on the pakfile
					if (!(*file = fopen (pak->filename, "rb")))
						Sys_Error ("Couldn't reopen %s", pak->filename);	
					fseek (*file, pak->files[i].filepos, SEEK_SET);
					com_filesize = pak->files[i].filelen;
					com_filefrompak = true;
                    com_filesearchpath = search->filename;

					file_from_pak = true;
					snprintf (com_netpath, sizeof(com_netpath), "%s#%i", pak->filename, i);
					return com_filesize;
				}
			}
		} else {
			snprintf (com_netpath, sizeof(com_netpath), "%s/%s", search->filename, filename);

			if (!(*file = fopen (com_netpath, "rb")))
				continue;

			if (developer.value)
				Sys_Printf ("FindFile: %s\n", com_netpath);

			com_filesize = COM_FileLength (*file);
			return com_filesize;
		}
	}

	if (developer.value)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	return -1;
}

//Filename are relative to the quake directory.
//Always appends a 0 byte to the loaded data.
static cache_user_t *loadcache;
static byte			*loadbuf;
static int			loadsize;
byte *FS_LoadFile (char *path, int usehunk) {
	FILE *h;
	byte *buf;
	char base[32];
	int len;

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = com_filesize = FS_FOpenFile (path, &h);
	if (!h)
		return NULL;

	// extract the filename base name for hunk tag
	COM_FileBase (path, base);

	if (usehunk == 1) {
		buf = (byte *) Hunk_AllocName (len + 1, base);
	} else if (usehunk == 2) {
		buf = (byte *) Hunk_TempAlloc (len + 1);
	} else if (usehunk == 0) {
		buf = (byte *) Z_Malloc (len + 1);
	} else if (usehunk == 3) {
		buf = (byte *) Cache_Alloc (loadcache, len + 1, base);
	} else if (usehunk == 4) {
		if (len+1 > loadsize)
			buf = (byte *) Hunk_TempAlloc (len + 1);
		else
			buf = loadbuf;
	} else {
		Sys_Error ("FS_LoadFile: bad usehunk");
	}

	if (!buf)
		Sys_Error ("FS_LoadFile: not enough space for %s", path);
		
	((byte *)buf)[len] = 0;
#ifndef SERVERONLY
	Draw_BeginDisc ();
#endif
	fread (buf, 1, len, h);
	fclose (h);
#ifndef SERVERONLY
	Draw_EndDisc ();
#endif

	return buf;
}

byte *FS_LoadHunkFile (char *path) {
	return FS_LoadFile (path, 1);
}

byte *FS_LoadTempFile (char *path) {
	return FS_LoadFile (path, 2);
}

void FS_LoadCacheFile (char *path, struct cache_user_s *cu) {
	loadcache = cu;
	FS_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
byte *FS_LoadStackFile (char *path, void *buffer, int bufsize) {
	byte *buf;
	
	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = FS_LoadFile (path, 4);
	
	return buf;
}

/*
Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
*/
pack_t *FS_LoadPackFile (char *packfile) {
	dpackheader_t header;
	int i;
	packfile_t *newfiles;
	pack_t *pack;
	FILE *packhandle;
	dpackfile_t *info;

	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	pack = (pack_t *) Q_malloc (sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = header.dirlen / sizeof(dpackfile_t);

	pack->files = newfiles = (packfile_t *) Q_malloc (pack->numfiles * sizeof(packfile_t));
	info = (dpackfile_t *) Q_malloc (header.dirlen);

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (info, 1, header.dirlen, packhandle);

	// parse the directory
	for (i = 0; i < pack->numfiles; i++) {
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	Q_free(info);
	return pack;
}
// QW262 -->
#ifndef SERVERONLY
/*
================
COM_SetUserDirectory
================
*/
void COM_SetUserDirectory (char *dir, char* type)
{
	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") ) {
			Com_Printf ("Userdir should be a single filename, not a path\n");
			return;
		}
	strcpy(userdirfile, dir);
	userdir_type = Q_atoi(type);
	com_gamedirfile[0]='\0'; // force reread
}
#endif

qbool FS_AddPak (char *pakfile)
{
	searchpath_t *search;
	pack_t *pak;

	pak = FS_LoadPackFile (pakfile);
	if (!pak)
		return false;

	//search = Hunk_Alloc (sizeof(searchpath_t));
	search = (searchpath_t *) Q_malloc(sizeof(searchpath_t));
	search->pack = pak;
	search->next = com_searchpaths;
	com_searchpaths = search;		
	return true;
}
#ifndef SERVERONLY

void FS_AddUserPaks (char *dir)
{
	FILE	*f;
	char	pakfile[MAX_OSPATH];
	char	userpak[32];

	// add paks listed in paks.lst
	sprintf (pakfile, "%s/pak.lst", dir);
	f = fopen(pakfile, "r");
	if (f){
		int len;
		while (1){
			if (!fgets(userpak, 32, f))
				break;
			len = strlen(userpak);
			// strip endline
			if (userpak[len-1] == '\n') {
				userpak[len-1] = '\0';
				--len;
			}
			if (userpak[len-1] == '\r') {
				userpak[len-1] = '\0';
				--len;
			}
			if (len < 5)
				continue;
#ifdef GLQUAKE
			if (!strncasecmp(userpak,"soft",4))
				continue;
#else
			if (!strncasecmp(userpak,"gl", 2))
				continue;
#endif
			sprintf (pakfile, "%s/%s", dir, userpak);
			FS_AddPak(pakfile);
		}
		fclose(f);
	}
// add userdir.pak
	if (UserdirSet) {
		sprintf (pakfile, "%s/%s.pak", dir, userdirfile);
		FS_AddPak(pakfile);
	}
}
#endif

// <-- QW262

//Sets com_gamedir, adds the directory to the head of the path, then loads and adds pak1.pak pak2.pak ... 
void FS_AddGameDirectory (char *dir) {
	int i;
	searchpath_t *search;
	char pakfile[MAX_OSPATH], *p;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(com_gamedirfile, ++p);
	else
		strcpy(com_gamedirfile, p);
	strcpy (com_gamedir, dir);

	// add the directory to the search path
	search = (searchpath_t *) Q_malloc (sizeof(searchpath_t));
	strcpy (search->filename, com_gamedir);
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0; ; i++) {
		snprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		if(!FS_AddPak(pakfile))
			break;
	}
#ifndef SERVERONLY
// other paks
	FS_AddUserPaks (dir);
#endif
}

void FS_AddUserDirectory ( char *dir ) {
	int i;
	searchpath_t *search;
	char pakfile[MAX_OSPATH];

	if ( !UserdirSet )
	        return;
      	switch (userdir_type) {
      	case 0:	snprintf (com_userdir, sizeof(com_userdir), "%s/%s", com_gamedir, userdirfile); break;
      	case 1:	snprintf (com_userdir, sizeof(com_userdir), "%s/%s/%s", com_basedir, userdirfile, dir); break;
      	case 2: snprintf (com_userdir, sizeof(com_userdir), "%s/qw/%s/%s", com_basedir, userdirfile, dir); break;
      	case 3: snprintf (com_userdir, sizeof(com_userdir), "%s/qw/%s", com_basedir, userdirfile); break;
      	case 4: snprintf (com_userdir, sizeof(com_userdir), "%s/%s", com_basedir, userdirfile); break;
      	case 5:
      		{
      		char* homedir = getenv("HOME");
      		if (homedir)
      			snprintf (com_userdir, sizeof(com_userdir), "%s/qw/%s", homedir, userdirfile);
      		break;
      		}
      	default:
      	      return;  
      	}

	// add the directory to the search path
	search = (searchpath_t *) Q_malloc (sizeof(searchpath_t));
	strcpy (search->filename, com_userdir);
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0; ; i++) {
		snprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", com_userdir, i);
		if(!FS_AddPak(pakfile))
			break;
	}
#ifndef SERVERONLY
// other paks
	FS_AddUserPaks (com_userdir);
#endif
}

//Sets the gamedir and path to a different directory.
void FS_SetGamedir (char *dir) {
	searchpath_t  *next;
	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") ) {
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp(com_gamedirfile, dir))
		return;		// still the same
	strlcpy (com_gamedirfile, dir, sizeof(com_gamedirfile));

	// free up any current game dir info
	while (com_searchpaths != com_base_searchpaths)	{
		if (com_searchpaths->pack) {
			fclose (com_searchpaths->pack->handle);
			Q_free (com_searchpaths->pack->files);
			Q_free (com_searchpaths->pack);
		}
		next = com_searchpaths->next;
		Q_free (com_searchpaths);
		com_searchpaths = next;
	}

	// flush all data, so it will be forced to reload
	Cache_Flush ();

	//sprintf (com_gamedir, "%s/%s", com_basedir, dir);

	if (strcmp(dir, "id1") && strcmp(dir, "qw") && strcmp(dir, "ezquake"))	
	{
		FS_AddGameDirectory ( va("%s/%s", com_basedir, dir) );
	}
// QW262 -->
#ifndef SERVERONLY
        FS_AddUserDirectory(dir);
#endif
// <-- QW262
}

void FS_InitFilesystem (void) {
	int i;

	// -basedir <path>
	// Overrides the system supplied base directory (under id1)
	if ((i = COM_CheckParm ("-basedir")) && i < com_argc - 1)
		strlcpy (com_basedir, com_argv[i + 1], sizeof(com_basedir));
	else
		_getcwd(com_basedir, sizeof(com_basedir) - 1);

	for (i = 0; i < strlen(com_basedir); i++)
		if (com_basedir[i] == '\\')
			com_basedir[i] = '/';

	i = strlen(com_basedir) - 1;
	if (i >= 0 && com_basedir[i] == '/')
		com_basedir[i] = 0;

	// start up with id1 by default
	FS_AddGameDirectory ( va("%s/id1", com_basedir) );
	FS_AddGameDirectory ( va("%s/ezquake", com_basedir) );	
	FS_AddGameDirectory ( va("%s/qw", com_basedir) );

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	i = COM_CheckParm("-userdir");
	if (i && i < com_argc - 2) {
		COM_SetUserDirectory(com_argv[i+1], com_argv[i+2]);
	}

	// the user might want to override default game directory
	if (!(i = COM_CheckParm ("-game")))
		i = COM_CheckParm ("+gamedir");
	if (i && i < com_argc - 1)
		FS_SetGamedir (com_argv[i + 1]);
	else
	{ 
		if( UserdirSet ) 
			FS_SetGamedir("qw");
	}
}

/*
=====================================================================
  INFO STRINGS
=====================================================================
*/

//Searches the string for the given key and returns the associated value, or an empty string.
char *Info_ValueForKey (char *s, char *key) {
	char pkey[512];
	static char value[4][512];	// use two buffers so compares work without stomping on each other
	static int	valueindex;
	char *o;

	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1) {
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s) {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key) {
	char *start, pkey[512], value[512], *o;

	if (strstr (key, "\\")) {
		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) ) {
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix) {
	char *s, pkey[512], value[512], *o;

	s = start;

	while (1) {
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix) {
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}

void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize) {
	char new[1024], *v;
	int c;
#ifndef CLIENTONLY
	extern cvar_t sv_highchars;
#endif

	if (strstr (key, "\\") || strstr (value, "\\") ) {
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") ) {
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > 63 || strlen(value) > 63) {
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}

	// this next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key))) {
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if (strlen(value) - strlen(v) + strlen(s) >= maxsize) {
			Com_Printf ("Info string length exceeded\n");
			return;
		}
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	sprintf (new, "\\%s\\%s", key, value);

	if ((strlen(new) + strlen(s)) >= maxsize) {
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = new;
#ifndef CLIENTONLY
	if (!sv_highchars.value) {
		while (*v) {
			c = (unsigned char)*v++;
			if (c == ('\\'|128))
				continue;
			c &= 127;
			if (c >= 32)
				*s++ = c;
		}
	} else
#endif
	{
		while (*v) {
			c = (unsigned char)*v++;
			if (c > 13)
				*s++ = c;
		}
		*s = 0;
	}
}

void Info_SetValueForKey (char *s, char *key, char *value, int maxsize) {
	if (key[0] == '*') {
		Com_Printf ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

#define INFO_PRINT_FIRST_COLUMN_WIDTH 20
void Info_Print (char *s) {
	char key[512], value[512], *o;
	int l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < INFO_PRINT_FIRST_COLUMN_WIDTH) {
			memset (o, ' ', INFO_PRINT_FIRST_COLUMN_WIDTH - l);
			key[INFO_PRINT_FIRST_COLUMN_WIDTH] = 0;
		} else {
			*o = 0;
		}
		Com_Printf ("%s", key);

		if (!*s) {
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

//============================================================================

static byte chktbl[1024] = {
0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3,

// Only the first 512 bytes of the table are initialized, the rest
// is just zeros.
// This is an idiocy in QW but we can't change this, or checksums
// will not match.
};

//For proxy protecting
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence) {
	unsigned short crc;
	byte *p;
	byte chkb[60 + 4];

	p = chktbl + ((unsigned int)sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block (chkb, length);

	crc &= 0xff;

	return crc;
}


//=====================================================================

#define	MAXPRINTMSG	4096

void (*rd_print) (char *) = NULL;

void Com_BeginRedirect (void (*RedirectedPrint) (char *)) {
	rd_print = RedirectedPrint;
}

void Com_EndRedirect (void) {
	rd_print = NULL;
}

//All console printing must go through this in order to be logged to disk
unsigned	Print_flags[16];
int		Print_current = 0;
void Com_Printf (char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];
	
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	if (rd_print) {
		// add to redirected message
		rd_print (msg);
		return;
	}

	// also echo to debugging console
	Sys_Printf ("%s", msg);

// Triggers with mask 64
	if (!(Print_flags[Print_current] & PR_TR_SKIP))
		CL_SearchForReTriggers (msg, RE_PRINT_INTERNAL);

	// write it to the scrollable buffer
//	Con_Print (va("ezQuake: %s", msg));
	Con_Print (msg);

	Print_flags[Print_current] = 0;
}

//A Com_Printf that only shows up if the "developer" cvar is set
void Com_DPrintf (char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	Print_flags[Print_current] |= PR_TR_SKIP;
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);
	
	Com_Printf ("%s", msg);
}

int isspace2(int c)
{
    if (c == 0x09 || c == 0x0D || c == 0x0A || c == 0x20)
        return 1;
    return 0;
} 

