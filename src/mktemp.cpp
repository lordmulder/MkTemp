/******************************************************************************/
/* MkTemp, by LoRd_MuldeR <MuldeR2@GMX.de>                                    */
/* This work has been released under the CC0 1.0 Universal license!           */
/******************************************************************************/

#define WIN32_LEAN_AND_MEAN 1

#include <Windows.h>
#include <ShellAPI.h>
#include <Shlobj.h>

#define MAX_FAIL 2477U

/* ======================================================================= */
/* Debug outputs                                                           */
/* ======================================================================= */

#define TRACE(STR) do \
{ \
	if(param->debug_output) print_text(std_err, (STR)); \
} \
while(0)

#define TRACE_FMT(FMT, LEN, ...) do \
{ \
	if(param->debug_output) \
	{ \
		if(wchar_t *const _trace = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * (LEN))) \
		{ \
			if(format_str(_trace, (FMT), __VA_ARGS__)) \
			{ \
				print_text_utf16(std_err, _trace, CP_UTF8); \
			} \
			LocalFree(_trace); \
		} \
	} \
} \
while(0)

/* ======================================================================= */
/* String Routines                                                         */
/* ======================================================================= */

static CHAR *utf16_to_bytes(const WCHAR *const input, const UINT code_page)
{
	CHAR *buffer;
	DWORD buffer_size = 0U, result = 0U;

	buffer_size = WideCharToMultiByte(code_page, 0, input, -1, NULL, 0, NULL, NULL);
	if(buffer_size < 1U)
	{
		return NULL;
	}

	buffer = (CHAR*) LocalAlloc(LPTR, sizeof(BYTE) * buffer_size);
	if(!buffer)
	{
		return NULL;
	}

	result = WideCharToMultiByte(code_page, 0, input, -1, (LPSTR)buffer, buffer_size, NULL, NULL);
	if((result > 0U) && (result <= buffer_size))
	{
		return buffer;
	}

	LocalFree(buffer);
	return NULL;
}

static BOOL format_str(wchar_t *const buffer, const wchar_t *const format, ...)
{
	BOOL result = FALSE;
	va_list ap;
	va_start(ap, format);
	if(wvsprintfW(buffer, format, ap) > 0L)
	{
		result = TRUE;
	}
	va_end(ap);
	return result;
}

static void remove_trailing_sep(wchar_t *const path)
{
	DWORD len = lstrlenW(path);
	while((len > 1U) && ((path[len - 1U] == L'/') || (path[len - 1U] == L'\\')))
	{
		path[--len] = L'\0';
	}
}

static wchar_t *concat_str(const wchar_t *const str1, const wchar_t *const str2)
{
	if(str1 && str2)
	{
		wchar_t *const buffer = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * (lstrlenW(str1) + lstrlenW(str2) + 1U));
		if(buffer)
		{
			lstrcpyW(buffer, str1);
			lstrcatW(buffer, str2);
			return buffer;
		}
	}
	return NULL;
}

/* ======================================================================= */
/* Text Output                                                             */
/* ======================================================================= */

static __inline BOOL print_text(const HANDLE output, const CHAR *const text)
{
	DWORD bytes_written;
	if(output != INVALID_HANDLE_VALUE)
	{
		return WriteFile(output, text, lstrlenA(text), &bytes_written, NULL);
	}
	return TRUE;
}

static __inline BOOL print_text_utf16(const HANDLE output, const wchar_t *const text, const UINT code_page)
{
	if(output != INVALID_HANDLE_VALUE)
	{
		BOOL result = FALSE;
		if(CHAR *const byte_str = utf16_to_bytes(text, code_page))
		{
			result = print_text(output, byte_str);
		}
		return result;
	}
	return TRUE;
}

/* ======================================================================= */
/* Environment                                                             */
/* ======================================================================= */

static wchar_t *get_environment_variable(const wchar_t *const name)
{
	DWORD buff_len = 0U;
	wchar_t *buffer = NULL;

	for(;;)
	{
		const DWORD result = GetEnvironmentVariableW(name, buffer, buff_len);
		if(result > 0U)
		{
			if(result < buff_len)
			{
				return buffer; /*found!*/
			}
			else
			{
				if(buffer)
				{
					LocalFree(buffer);
				}
				buffer = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * (buff_len = result));
				if(!buffer)
				{
					return NULL;
				}
			}
		}
		else
		{
			if(buffer)
			{
				LocalFree(buffer);
			}
			return NULL;
		}
	}
}

/* ======================================================================= */
/* File System                                                             */
/* ======================================================================= */

static wchar_t *get_full_path(const wchar_t *const path)
{
	DWORD buff_len = 0U;
	wchar_t *buffer = NULL;

	if(!path)
	{
		return NULL;
	}

	for(;;)
	{
		const DWORD result = GetFullPathName(path, buff_len, buffer, NULL);
		if(result > 0U)
		{
			if(result < buff_len)
			{
				remove_trailing_sep(buffer);
				return buffer;
			}
			else
			{
				if(buffer)
				{
					LocalFree(buffer);
				}
				buffer = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * (buff_len = result));
				if(!buffer)
				{
					return NULL;
				}
			}
		}
		else
		{
			if(buffer)
			{
				LocalFree(buffer);
			}
			return NULL;
		}
	}
}

static LONG directory_exists(const wchar_t *const path)
{
	const DWORD attribs = GetFileAttributesW(path);
	if(attribs != INVALID_FILE_ATTRIBUTES)
	{
		return (attribs & FILE_ATTRIBUTE_DIRECTORY) ? 1L : (-1L);
	}
	return 0L;
}

static BOOL try_create_directory(const wchar_t *const path)
{
	DWORD fail_count = 0U;
	LONG result = directory_exists(path);
	while(result < 1L)
	{
		if((result < 0L) || (++fail_count > 31U))
		{
			return FALSE; /*exists, but it is a file (or failed too often)*/
		}
		if(!CreateDirectoryW(path, NULL))
		{
			const DWORD error = GetLastError();
			switch(error)
			{
			case ERROR_PATH_NOT_FOUND:
				SHCreateDirectoryExW(NULL, path, NULL); /*must create recursively*/
				break;
			case ERROR_BAD_PATHNAME:
			case ERROR_INVALID_NAME:
				return FALSE; /*the directory syntax is invalid*/
			}
		}
		result = directory_exists(path);
	}
	return TRUE;
}

/* ======================================================================= */
/* Path Detection                                                          */
/* ======================================================================= */

static wchar_t *get_environment_path(const wchar_t *const variable_name)
{
	wchar_t *env_value = get_environment_variable(variable_name);
	if(env_value)
	{
		if(env_value[0U])
		{
			wchar_t *const path_full = get_full_path(env_value);
			if(path_full)
			{
				if(try_create_directory(path_full))
				{
					LocalFree(env_value);
					return path_full;
				}
				else
				{
					LocalFree(path_full);
				}
			}
		}
		LocalFree(env_value);
	}
	return NULL;
}

static wchar_t *get_shell_folder_path(const int csidl, const wchar_t *const suffix)
{
	wchar_t *const buffer = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * MAX_PATH);
	if(!buffer)
	{
		return NULL;
	}
	
	if(SHGetFolderPathW(NULL, csidl | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, buffer) == S_OK)
	{
		remove_trailing_sep(buffer);
		if(try_create_directory(buffer))
		{
			if(suffix && suffix[0U])
			{
				wchar_t *const temp_path = concat_str(buffer, suffix);
				if(temp_path)
				{
					if(try_create_directory(temp_path))
					{
						LocalFree(buffer);
						return temp_path;
					}
					else
					{
						LocalFree(temp_path);
					}
				}
			}
			else
			{
				return buffer;
			}
		}
	}

	LocalFree(buffer);
	return NULL;
}

static wchar_t *get_current_directory(void)
{
	DWORD buff_len = 0U;
	wchar_t *buffer = NULL;

	for(;;)
	{
		const DWORD result = GetCurrentDirectoryW(buff_len, buffer);
		if(result > 0U)
		{
			if(result < buff_len)
			{
				remove_trailing_sep(buffer);
				return buffer;
			}
			else
			{
				if(buffer)
				{
					LocalFree(buffer);
				}
				buffer = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * (buff_len = result));
				if(!buffer)
				{
					return NULL;
				}
			}
		}
		else
		{
			if(buffer)
			{
				LocalFree(buffer);
			}
			return NULL;
		}
	}
}

static wchar_t *get_user_directory(const wchar_t *const path)
{
	if(path && path[0U])
	{
		wchar_t *const path_full = get_full_path(path);
		if(path_full)
		{
			if(try_create_directory(path_full))
			{
				return path_full;
			}
			else
			{
				LocalFree(path_full);
			}
		}
	}
	return NULL;
}

/* ======================================================================= */
/* Random Numbers                                                          */
/* ======================================================================= */

typedef struct random_t
{
	DWORD a, b, c, d;
	DWORD counter;
}
random_t;

static void random_seed(random_t *const state)
{
	LARGE_INTEGER counter;
	FILETIME time;
	state->a = 65599U * GetCurrentThreadId() + GetCurrentProcessId();
	do
	{
		GetSystemTimeAsFileTime(&time);
		QueryPerformanceCounter(&counter);
		state->b = GetTickCount();
		state->c = 65599U * time.dwHighDateTime + time.dwLowDateTime;
		state->d = 65599U * counter.HighPart + counter.LowPart;
	}
	while((!state->a) && (!state->b) && (!state->c) && (!state->d));
	state->counter = 0U;
}

static __inline DWORD random_next(random_t *const state)
{
	DWORD t = state->d;
	const DWORD s = state->a;
	state->d = state->c;
	state->c = state->b;
	state->b = s;
	t ^= t >> 2;
	t ^= t << 1;
	t ^= s ^ (s << 4);
	state->a = t;
	return t + (state->counter += 362437U);
}

/* ======================================================================= */
/* Parameters                                                              */
/* ======================================================================= */

typedef struct
{
	BOOL print_manpage, make_directory, debug_output;
	const wchar_t *user_directory;
}
param_t;

static BOOL validate_suffix(const HANDLE std_err, const wchar_t *const suffix)
{
	const DWORD len = lstrlenW(suffix);
	if((len < 1U) || (len > 5U))
	{
		print_text(std_err, "Error: Suffix must be 1 to 5 characters in length!\n");
		return FALSE;
	}

	for(DWORD i = 0U; i < len; ++i)
	{
		if(
			(suffix[i] == L'<') || (suffix[i] == L'>')  || (suffix[i] == L':') || (suffix[i] == L'.') || (suffix[i] == L'"') ||
			(suffix[i] == L'/') || (suffix[i] == L'\\') || (suffix[i] == L'|') || (suffix[i] == L'?') || (suffix[i] == L'*')
		)
		{
			print_text(std_err, "Error: Suffix contains an invalid character!\n");
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL parse_parameters(const HANDLE std_err, int *const arg_offset, const int argc, const wchar_t *const *const argv, param_t *const param)
{
	SecureZeroMemory(param, sizeof(param_t));

	while(*arg_offset < argc)
	{
		if((argv[*arg_offset][0U] == L'-') || (argv[*arg_offset][1U] == L'-'))
		{
			const wchar_t *const arg_name = argv[(*arg_offset)++] + 2U;
			if(!(*arg_name))
			{
				break; /*stop parsing*/
			}
			else if((!lstrcmpiW(arg_name, L"help")) || (!lstrcmpiW(arg_name, L"version")))
			{
				param->print_manpage = TRUE;
			}
			else if(!lstrcmpiW(arg_name, L"mkdir"))
			{
				param->make_directory = TRUE;
			}
			else if(!lstrcmpiW(arg_name, L"path"))
			{
				if((*arg_offset < argc) && argv[*arg_offset] && argv[*arg_offset][0U])
				{
					param->user_directory = argv[(*arg_offset)++];
				}
				else
				{
					print_text(std_err, "Error: Option '--path' is missing a required argument!\n");
					return FALSE;
				}
			}
			else if(!lstrcmpiW(arg_name, L"debug"))
			{
				param->debug_output = TRUE;
			}
			else
			{
				print_text(std_err, "Error: Unsupported option encountered!\n");
				return FALSE;
			}
		}
		else
		{
			break;
		}
	}

	return TRUE;
}

/* ======================================================================= */
/* Create Temporaty File/Directory                                         */
/* ======================================================================= */

static const wchar_t *const TEMPLATE = L"%s\\~%07lX.%s";

static BOOL generate_file(const HANDLE std_err, const param_t *const param, wchar_t *const buffer, const wchar_t *const path, const wchar_t *const suffix, random_t *const rnd)
{
	DWORD fail_count = 0U;

	for(UINT i = 0U; i < 0x7FFFFFFF; ++i)
	{
		if(format_str(buffer, TEMPLATE, path, random_next(rnd) & 0xFFFFFFF, suffix))
		{
			TRACE_FMT(L"Trying to create file: \"%s\"\n", lstrlenW(buffer) + 28U, buffer);
			const HANDLE handle = CreateFile(buffer, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, 0U, NULL);
			if(handle != INVALID_HANDLE_VALUE)
			{
				TRACE("Success.\n");
				CloseHandle(handle);
				return TRUE;
			}
			else
			{
				const DWORD error = GetLastError();
				if((error != ERROR_FILE_EXISTS) && (error != ERROR_ALREADY_EXISTS))
				{
					TRACE("Failed!\n");
					if(++fail_count > MAX_FAIL)
					{
						TRACE("Giving up!\n");
						return FALSE;
					}
				}
				else
				{
					TRACE("Already exists!\n");
				}
			}
		}
	}

	TRACE("Giving up!\n");
	return FALSE;
}

static BOOL generate_directory(const HANDLE std_err, const param_t *const param, wchar_t *const buffer, const wchar_t *const path, const wchar_t *const suffix, random_t *const rnd)
{
	DWORD fail_count = 0U;

	for(UINT i = 0U; i < 0x7FFFFFFF; ++i)
	{
		if(format_str(buffer, TEMPLATE, path, random_next(rnd) & 0xFFFFFFF, suffix))
		{
			TRACE_FMT(L"Trying to create directory: \"%s\"\n", lstrlenW(buffer) + 33U, buffer);
			if(CreateDirectoryW(buffer, NULL))
			{
				TRACE("Success.\n");
				return TRUE;
			}
			else
			{
				const DWORD error = GetLastError();
				if((error != ERROR_FILE_EXISTS) && (error != ERROR_ALREADY_EXISTS))
				{
					TRACE("Failed!\n");
					if(++fail_count > MAX_FAIL)
					{
						TRACE("Giving up!\n");
						return FALSE;
					}
				}
				else
				{
					TRACE("Already exists!\n");
				}
			}
		}
	}

	TRACE("Giving up!\n");
	return FALSE;
}

static wchar_t *generate(const HANDLE std_err, const param_t *const param, const LONG index, const wchar_t *const suffix, random_t *const rnd)
{
	wchar_t *base_path = NULL;
	switch(index)
	{
	case 0L:
		TRACE("Trying environment variable %TMP%...\n");
		base_path = get_environment_path(L"TMP");
		break;
	case 1L:
		TRACE("Trying environment variable %TEMP%...\n");
		base_path = get_environment_path(L"TEMP");
		break;
	case 2L:
		TRACE("Trying shell directory CSIDL_LOCAL_APPDATA...\n");
		base_path = get_shell_folder_path(CSIDL_LOCAL_APPDATA, L"\\Temp");
		break;
	case 3L:
		TRACE("Trying shell directory CSIDL_PROFILE...\n");
		base_path = get_shell_folder_path(CSIDL_PROFILE, L"\\.cache");
		break;
	case 4L:
		TRACE("Trying shell directory CSIDL_PERSONAL...\n");
		base_path = get_shell_folder_path(CSIDL_PERSONAL, NULL);
		break;
	case 5L:
		TRACE("Trying shell directory CSIDL_DESKTOPDIRECTORY...\n");
		base_path = get_shell_folder_path(CSIDL_DESKTOPDIRECTORY, NULL);
		break;
	case 6L:
		TRACE("Trying shell directory CSIDL_WINDOWS...\n");
		base_path = get_shell_folder_path(CSIDL_WINDOWS, L"\\Temp");
		break;
	case 7L:
		TRACE("Trying current directory...\n");
		base_path = get_current_directory(); /*last fallback*/
		break;
	default:
		TRACE("Trying user-defined directory...\n");
		base_path = get_user_directory(param->user_directory); /*user-specified*/
		break;
	}

	if(!base_path)
	{
		TRACE("Failed!\n");
		return NULL; /*failed*/
	}

	TRACE_FMT(L"Using directory: \"%s\"\n", lstrlenW(base_path) + 21U, base_path);

	wchar_t *const buffer = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * (lstrlenW(base_path) + lstrlenW(suffix) + 12U));
	if(!buffer)
	{
		TRACE("Memory allocation has failed!\n");
		LocalFree(base_path);
		return NULL; /*malloc failed*/
	}

	if(param->make_directory ? generate_directory(std_err, param, buffer, base_path, suffix, rnd) : generate_file(std_err, param, buffer, base_path, suffix, rnd))
	{
		const DWORD len = lstrlenW(buffer);
		buffer[len] = L'\n';
		buffer[len + 1U] = L'\0';
		LocalFree(base_path);
		return buffer;
	}

	LocalFree(buffer);
	LocalFree(base_path);
	return NULL;
}

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

#define ARG_OFFSET (make_directory ? 2 : 1)

static UINT mktemp_main(const int argc, const wchar_t *const *const argv)
{
	UINT result = 1U, prev_cp = 0U;
	int arg_offset = 1;
	wchar_t *generated_path = NULL;
	CHAR *generated_path_utf8 = NULL;

	const HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
	const HANDLE std_err = GetStdHandle(STD_ERROR_HANDLE);

	param_t param;
	if(!parse_parameters(std_err, &arg_offset, argc, argv, &param))
	{
		return 1U;
	}

	if(param.print_manpage)
	{
		print_text(std_err, "mktemp [" __DATE__ "], by LoRd_MuldeR <MuldeR2@GMX.de>\n");
		print_text(std_err, "Generates a unique temporary file name or temporary sub-directory.\n\n");
		print_text(std_err, "Usage:\n");
		print_text(std_err, "  mktemp.exe [--mkdir] [--path <path>] [<suffix>]\n\n");
		print_text(std_err, "Options:\n");
		print_text(std_err, "  --mkdir  Create a temporary sub-directory instead of a file\n");
		print_text(std_err, "  --path   Use the specified path, instead of the system's TEMP path\n");
		print_text(std_err, "  --debug  Generate additional debug output (written to stderr)\n\n");
		print_text(std_err, "If 'suffix' is not specified, the default suffix '*.tmp' is used!\n\n");
		return 0U;
	}

	const wchar_t *const suffix = (arg_offset < argc) ? argv[arg_offset] : L"tmp";
	if(!validate_suffix(std_err, suffix))
	{
		return 1U;
	}

	if(GetFileType(std_out) == FILE_TYPE_CHAR)
	{
		prev_cp = GetConsoleOutputCP();
		SetConsoleOutputCP(CP_UTF8);
	}

	random_t rnd;
	random_seed(&rnd);

	if(param.user_directory)
	{
		if(generated_path = generate(std_err, &param, -1L, suffix, &rnd))
		{
			goto success;
		}
	}
	else
	{
		for(LONG index = 0L; index < 8L; ++index)
		{
			if(generated_path = generate(std_err, &param, index, suffix, &rnd))
			{
				goto success;
			}
		}
	}
	
	print_text(std_err, param.make_directory ? "Error: Failed to generate temporary directory!\n" : "Error: Failed to generate temporary file!\n");
	goto clean_up;

success:

	generated_path_utf8 = utf16_to_bytes(generated_path, CP_UTF8);
	if(!generated_path_utf8)
	{
		print_text(std_err, "Error: Failed to convert file name to UFT-8 format!\n");
		goto clean_up;
	}

	print_text(std_out, generated_path_utf8);
	result = 0U;

clean_up:

	if(generated_path)
	{
		LocalFree((HLOCAL)generated_path);
	}

	if(generated_path_utf8)
	{
		LocalFree((HLOCAL)generated_path_utf8);
	}

	if(prev_cp)
	{
		SetConsoleOutputCP(prev_cp);
	}

	return result;
}

/* ======================================================================= */
/* Entry point                                                             */
/* ======================================================================= */

void _entryPoint(void)
{
	int argc;
	const wchar_t *const *argv;
	UINT result = (UINT)(-1);

	SetErrorMode(SetErrorMode(0x3) | 0x3);

	if(argv = CommandLineToArgvW(GetCommandLineW(), &argc))
	{
		result = mktemp_main(argc, argv);
		LocalFree((HLOCAL)argv);
	}

	ExitProcess(result);
}
