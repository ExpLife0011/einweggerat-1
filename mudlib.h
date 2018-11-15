#pragma once
#include <Windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>

#pragma comment(lib, "crypt32.lib") 

#ifdef MUDLIB_IMPLEMENTATION
#undef MUDLIB_IMPLEMENTATION

class Mud_Base64
{
public:
    static TCHAR* encode(const BYTE* buf, unsigned int buflen, unsigned * outlen)
    {
        DWORD out_sz;
        TCHAR* out = NULL;
        if (CryptBinaryToString(buf, buflen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out, &out_sz)) {
            out = (TCHAR*)malloc((out_sz) * sizeof(TCHAR));
            ZeroMemory(out, (out_sz) * sizeof(TCHAR));
            if (CryptBinaryToString(buf, buflen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out, &out_sz))
            {
                *outlen = out_sz;
                return out;
            }
            else
                return NULL;
           
        }
        else
        return NULL;
        
    }
    static BYTE* decode(const TCHAR * string,int inlen, unsigned int * outlen)
    {
        BYTE *result = NULL;
        DWORD out_sz = 0;
        
        if (CryptStringToBinary(string, inlen, CRYPT_STRING_BASE64, NULL, &out_sz, NULL, NULL))
        {
            result = (BYTE*)malloc((out_sz)*sizeof(BYTE));
            ZeroMemory(result, (out_sz) * sizeof(BYTE));
            CryptStringToBinary(string, inlen, CRYPT_STRING_BASE64, (BYTE*)result, &out_sz, NULL, NULL);
        }
        else
        {
            return NULL;
        }
        *outlen = out_sz;
        return result;
    }
};

class Mud_Timing
{
private:
    LARGE_INTEGER start;                   //
    LARGE_INTEGER end;                     //
    double perf_inv;
    void init_perfinv()
    {
        LARGE_INTEGER freq = {};
        if (!::QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
            return;
        perf_inv = 1000000. / (double)freq.QuadPart;
    }
public:
    void timer_start()
    {
        QueryPerformanceCounter(&start);
    }

    long long timer_elapsedusec()
    {
        QueryPerformanceCounter(&end);
        long long starttime = start.QuadPart * perf_inv;
        long long endtime = end.QuadPart * perf_inv;
        return endtime - starttime;
    }

    long long milliseconds_now() {
        return microseconds_now() * 0.001;
    }

    long long seconds_now()
    {
        return microseconds_now() *0.000001;
    }

    long long microseconds_now() {
        LARGE_INTEGER ts = {};
        if (!::QueryPerformanceCounter(&ts))
            return 0;
        return (uint64_t)(perf_inv * ts.QuadPart);
    }

    Mud_Timing()
    {
        perf_inv = 0;
        init_perfinv();
    }

    ~Mud_Timing()
    {
        
    }
};

class Mud_FileAccess
{
public:
    static unsigned get_filesize(const TCHAR *path)
    {
        FILE *input = _wfopen(path, L"rb");
        if (!input)return NULL;
        fseek(input, 0, SEEK_END);
        unsigned size = ftell(input);
        fclose(input);
        input = NULL;
        return size;
    }

    static bool save_data(unsigned char* data, unsigned size, TCHAR* path)
    {
        FILE *input = _wfopen(path, L"wb");
        if (!input)return false;
        fwrite(data, 1, size, input);
        fclose(input);
        input = NULL;
        return true;
    }

    static unsigned char* load_data(TCHAR* path, unsigned * size)
    {
        FILE *input = _wfopen(path, L"rb");
        if (!input)return NULL;
        fseek(input, 0, SEEK_END);
        unsigned Size = ftell(input);
        *size = Size;
        rewind(input);
        BYTE *Memory = (BYTE *)malloc(Size);
        int res = fread(Memory, 1, Size, input);
        fclose(input);
        if (!res)
        {
            free(Memory);
            return NULL;
        }
        return Memory;
    }
};

class Mud_MiscWindows
{
public:
    
    static void redirectiotoconsole()
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }

    static LPSTR* cmdlinetoargANSI(LPSTR lpCmdLine, INT *pNumArgs)
    {
        int retval;
        retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
        if (!SUCCEEDED(retval))
            return NULL;

        LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
        if (lpWideCharStr == NULL)
            return NULL;

        retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval);
        if (!SUCCEEDED(retval))
        {
            free(lpWideCharStr);
            return NULL;
        }

        int numArgs;
        LPWSTR* args;
        args = CommandLineToArgvW(lpWideCharStr, &numArgs);
        free(lpWideCharStr);
        if (args == NULL)
            return NULL;

        int storage = numArgs * sizeof(LPSTR);
        for (int i = 0; i < numArgs; ++i)
        {
            BOOL lpUsedDefaultChar = FALSE;
            retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
            if (!SUCCEEDED(retval))
            {
                LocalFree(args);
                return NULL;
            }

            storage += retval;
        }

        LPSTR* result = (LPSTR*)LocalAlloc(LMEM_FIXED, storage);
        if (result == NULL)
        {
            LocalFree(args);
            return NULL;
        }

        int bufLen = storage - numArgs * sizeof(LPSTR);
        LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
        for (int i = 0; i < numArgs; ++i)
        {
            assert(bufLen > 0);
            BOOL lpUsedDefaultChar = FALSE;
            retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
            if (!SUCCEEDED(retval))
            {
                LocalFree(result);
                LocalFree(args);
                return NULL;
            }

            result[i] = buffer;
            buffer += retval;
            bufLen -= retval;
        }

        LocalFree(args);

        *pNumArgs = numArgs;
        return result;
    }
};

#endif 