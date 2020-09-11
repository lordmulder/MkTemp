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
			lstrcpy(buffer, str1);
			lstrcat(buffer, str2);
			return buffer;
		}
	}
	return NULL;
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
	LONG result;
	while((result = directory_exists(path)) < 1L)
	{
		if((result < 0L) || (++fail_count > 31U))
		{
			return FALSE; /*exists, but it is a file (or failed too often)*/
		}
		if(fail_count > 16U)
		{
			Sleep(0U);
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
		wchar_t *const path_full = get_full_path(env_value);
		if(path_full)
		{
			if(try_create_directory(path_full))
			{
				LocalFree(env_value);
				return path_full;
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
/* Create Temporaty File/Directory                                         */
/* ======================================================================= */

static const wchar_t *const TEMPLATE = L"%s\\~%07lX.%s";

static BOOL create_temp_file(wchar_t *const buffer, const wchar_t *const path, const wchar_t *const suffix, random_t *const rnd)
{
	DWORD fail_count = 0U;

	for(UINT i = 0U; i < 0x7FFFFFFF; ++i)
	{
		format_str(buffer, TEMPLATE, path, random_next(rnd) & 0xFFFFFFF, suffix);
		const HANDLE handle = CreateFile(buffer, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, 0U, NULL);
		if(handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(handle);
			return TRUE;
		}
		else
		{
			const DWORD error = GetLastError();
			if((error != ERROR_FILE_EXISTS) && (error != ERROR_ALREADY_EXISTS))
			{
				if(++fail_count > MAX_FAIL)
				{
					return FALSE;
				}
			}
		}
	}

	return FALSE;
}

static BOOL create_temp_directory(wchar_t *const buffer, const wchar_t *const path, const wchar_t *const suffix, random_t *const rnd)
{
	DWORD fail_count = 0U;

	for(UINT i = 0U; i < 0x7FFFFFFF; ++i)
	{
		format_str(buffer, TEMPLATE, path, random_next(rnd) & 0xFFFFFFF, suffix);
		if(CreateDirectoryW(buffer, NULL))
		{
			return TRUE;
		}
		else
		{
			const DWORD error = GetLastError();
			if((error != ERROR_FILE_EXISTS) && (error != ERROR_ALREADY_EXISTS))
			{
				if(++fail_count > MAX_FAIL)
				{
					return FALSE;
				}
			}
		}
	}

	return FALSE;
}

static wchar_t *generate_temp_item(const UINT index, const BOOL make_directory, const wchar_t *const suffix, random_t *const rnd)
{
	wchar_t *base_path = NULL;
	switch(index)
	{
	case 0U:
		base_path = get_environment_path(L"TMP"); /*try first!*/
		break;
	case 1U:
		base_path = get_environment_path(L"TEMP");
		break;
	case 2U:
		base_path = get_shell_folder_path(CSIDL_LOCAL_APPDATA, L"\\Temp");
		break;
	case 3U:
		base_path = get_shell_folder_path(CSIDL_APPDATA, L"\\Temp");
		break;
	case 4U:
		base_path = get_shell_folder_path(CSIDL_PROFILE, L"\\.cache");
		break;
	case 5U:
		base_path = get_shell_folder_path(CSIDL_PERSONAL, NULL);
		break;
	case 6U:
		base_path = get_shell_folder_path(CSIDL_DESKTOPDIRECTORY, NULL);
		break;
	case 7U:
		base_path = get_current_directory(); /*last fallback*/
		break;
	}

	if(!base_path)
	{
		return NULL; /*failed*/
	}

#if _DEBUG
	OutputDebugStringW(base_path);
#endif //_DEBUG

	wchar_t *const buffer = (wchar_t*) LocalAlloc(LPTR, sizeof(wchar_t) * (lstrlenW(base_path) + lstrlenW(suffix) + 12U));
	if(!buffer)
	{
		LocalFree(base_path);
		return NULL; /*malloc failed*/
	}

	if(make_directory ? create_temp_directory(buffer, base_path, suffix, rnd) : create_temp_file(buffer, base_path, suffix, rnd))
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
		if(!(((suffix[i] >= L'A') && (suffix[i] <= L'Z')) || ((suffix[i] >= L'a') && (suffix[i] <= L'z')) || ((suffix[i] >= L'0') && (suffix[i] <= L'9')) || (suffix[i] == '_')))
		{
			print_text(std_err, "Error: Suffix contains an invalid character!\n");
			return FALSE;
		}
	}

	return TRUE;
}

/* ======================================================================= */
/* MAIN                                                                    */
/* ======================================================================= */

#define ARG_OFFSET (make_directory ? 2 : 1)

static UINT mktemp_main(const int argc, const wchar_t *const *const argv)
{
	UINT result = 1U, prev_cp = 0U;
	wchar_t *temp_item = NULL;
	CHAR *temp_item_utf8 = NULL;
	random_t rnd;

	const HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
	const HANDLE std_err = GetStdHandle(STD_ERROR_HANDLE);

	if(argc > 1)
	{
		if((lstrcmpiW(argv[1], L"--help") == 0) || (lstrcmpiW(argv[1], L"/?") == 0) || (lstrcmpiW(argv[1], L"-?") == 0))
		{
			print_text(std_err, "mktemp [" __DATE__ "]\n\n");
			print_text(std_err, "Usage:\n");
			print_text(std_err, "  mktemp.exe [--dir] [<suffix>]\n\n");
			return 0U;
		}
		else if(lstrcmpiW(argv[1], L"--version") == 0)
		{
			print_text(std_err, "mktemp [" __DATE__ "] [" __TIME__ "]\n");
			return 0U;
		}
	}

	const BOOL make_directory = (argc > 1) && (lstrcmpiW(argv[1], L"--dir") == 0);
	const wchar_t *const suffix = (argc > ARG_OFFSET) ? argv[ARG_OFFSET] : L"tmp";

	if(!validate_suffix(std_err, suffix))
	{
		goto clean_up;
	}

	if(GetFileType(std_out) == FILE_TYPE_CHAR)
	{
		prev_cp = GetConsoleOutputCP();
		SetConsoleOutputCP(CP_UTF8);
	}

	random_seed(&rnd);

	for(UINT index = 0U; index < 8U; ++index)
	{
		if(temp_item = generate_temp_item(index, make_directory, suffix, &rnd))
		{
			goto success;
		}
	}
	
	print_text(std_err, make_directory ? "Error: Failed to generate temporary directory!\n" : "Error: Failed to generate temporary file!\n");
	goto clean_up;

success:

	temp_item_utf8 = utf16_to_bytes(temp_item, CP_UTF8);
	if(!temp_item_utf8)
	{
		print_text(std_err, "Error: Failed to convert file name to UFT-8 format!\n");
		goto clean_up;
	}

	print_text(std_out, temp_item_utf8);
	result = 0U;

clean_up:

	if(temp_item)
	{
		LocalFree((HLOCAL)temp_item);
	}

	if(temp_item_utf8)
	{
		LocalFree((HLOCAL)temp_item_utf8);
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
