﻿

#include "port/port.h"
#include "leveldb/slice.h"
#include "util/logging.h"
#include "env_win32.h"

#include <Shlwapi.h>
#include <process.h>
#include <cstring>
#include <stdio.h>
#include <errno.h>
#include <io.h>
#include <DbgHelp.h>
#include <algorithm>
#pragma comment(lib,"DbgHelp.lib")


#ifdef max
#undef max
#endif

//Implementations
namespace leveldb
{

Win32Env g_Env;

namespace Win32
{

std::string GetCurrentDir()
{
    CHAR path[MAX_PATH];
    ::GetModuleFileNameA(::GetModuleHandleA(NULL),path,MAX_PATH);
    *strrchr(path,'\\') = 0;
    return std::string(path);
}

std::wstring GetCurrentDirW()
{
    WCHAR path[MAX_PATH];
    ::GetModuleFileNameW(::GetModuleHandleW(NULL),path,MAX_PATH);
    *wcsrchr(path,L'\\') = 0;
    return std::wstring(path);
}

std::string& ModifyPath(std::string& path)
{
    if(path[0] == '/' || path[0] == '\\')
    {
        path = CurrentDir + path;
    }
    std::replace(path.begin(),path.end(),'/','\\');

    return path;
}

std::wstring& ModifyPath(std::wstring& path)
{
    if(path[0] == L'/' || path[0] == L'\\')
    {
        path = CurrentDirW + path;
    }
    std::replace(path.begin(),path.end(),L'/',L'\\');
    return path;
}

std::string GetLastErrSz()
{
    LPVOID lpMsgBuf;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        0, // Default language
        (LPWSTR) &lpMsgBuf,
        0,
        NULL
    );
    std::string Err = WCharToMultiByte((LPCWSTR)lpMsgBuf);
    LocalFree( lpMsgBuf );
    return Err;
}

std::wstring GetLastErrSzW()
{
    LPVOID lpMsgBuf;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        0, // Default language
        (LPWSTR) &lpMsgBuf,
        0,
        NULL
    );
    std::wstring Err = (LPCWSTR)lpMsgBuf;
    LocalFree(lpMsgBuf);
    return Err;
}

WorkItemWrapper::WorkItemWrapper( ScheduleProc proc_,void* content_ ) :
    proc(proc_),pContent(content_)
{

}

DWORD WINAPI WorkItemWrapperProc(LPVOID pContent)
{
    WorkItemWrapper* item = static_cast<WorkItemWrapper*>(pContent);
    ScheduleProc TempProc = item->proc;
    void* arg = item->pContent;
    delete item;
    TempProc(arg);
    return 0;
}

/*
1.1 在 windows 7 + vc2005 + x86/x64 下输出为
[4180] Info    :  08-01 13:25:07. 72 # 4096 65536
*/
size_t GetPageSize()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return std::max(si.dwPageSize,si.dwAllocationGranularity);
}

const size_t g_PageSize = GetPageSize();


}



Env* Env::Default()
{
    return &g_Env;
}

Win32SequentialFile::Win32SequentialFile( const std::string& fname ) :
    _filename(fname),_hFile(NULL)
{
    _Init();
}

Win32SequentialFile::~Win32SequentialFile()
{
    _CleanUp();
}

Status Win32SequentialFile::Read( size_t n, Slice* result, char* scratch )
{
    Status sRet;
    DWORD hasRead = 0;
    //z 读取文件
    if(_hFile && ReadFile(_hFile,scratch,n,&hasRead,NULL) )
    {
        //z 从空间及其长度构造一个 Slice
        *result = Slice(scratch,hasRead);
    }
    else
    {
        sRet = Status::IOError(_filename, Win32::GetLastErrSz() );
    }
    return sRet;
}

//z 向后移动若干个位置。
Status Win32SequentialFile::Skip( uint64_t n )
{
    Status sRet;
    LARGE_INTEGER Move,NowPointer;
    Move.QuadPart = n;
    //z 使用 LARGE_INTEGER 支持大文件。
    if(!SetFilePointerEx(_hFile,Move,&NowPointer,FILE_CURRENT))
    {
        sRet = Status::IOError(_filename,Win32::GetLastErrSz());
    }
    return sRet;
}

BOOL Win32SequentialFile::isEnable()
{
    return _hFile ? TRUE : FALSE;
}

BOOL Win32SequentialFile::_Init()
{
    //z 打开文件？
    _hFile = CreateFileW(Win32::MultiByteToWChar(_filename.c_str() ),
                         GENERIC_READ,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
    return _hFile ? TRUE : FALSE;
}

void Win32SequentialFile::_CleanUp()
{
    //z 清理工作，关闭文件
    if(_hFile)
    {
        CloseHandle(_hFile);
        _hFile = NULL;
    }
}

Win32RandomAccessFile::Win32RandomAccessFile( const std::string& fname ) :
    _filename(fname),_hFile(NULL)
{
    _Init(Win32::MultiByteToWChar(fname.c_str() ) );
}

Win32RandomAccessFile::~Win32RandomAccessFile()
{
    _CleanUp();
}

Status Win32RandomAccessFile::Read(uint64_t offset,size_t n,Slice* result,char* scratch) const
{
    Status sRet;
    OVERLAPPED ol = {0};
    ZeroMemory(&ol,sizeof(ol));
    ol.Offset = (DWORD)offset;
    ol.OffsetHigh = (DWORD)(offset >> 32);
    DWORD hasRead = 0;
    if(!ReadFile(_hFile,scratch,n,&hasRead,&ol))
        sRet = Status::IOError(_filename,Win32::GetLastErrSz());
    else
        *result = Slice(scratch,hasRead);
    return sRet;
}

BOOL Win32RandomAccessFile::_Init( LPCWSTR path )
{
    BOOL bRet = FALSE;
    if(!_hFile)
        _hFile = ::CreateFileW(path,GENERIC_READ,0,NULL,OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,NULL);
    if(!_hFile || _hFile == INVALID_HANDLE_VALUE )
        _hFile = NULL;
    else
        bRet = TRUE;
    return bRet;
}

BOOL Win32RandomAccessFile::isEnable()
{
    return _hFile ? TRUE : FALSE;
}

void Win32RandomAccessFile::_CleanUp()
{
    if(_hFile)
    {
        ::CloseHandle(_hFile);
        _hFile = NULL;
    }
}

//z 取整假设y为3，x为11，11+3-1 = 13 13/3为4，4*3为12，取整
//z 假设y为3，x为0，1，
//z 函数含义：返回不小于x的最小的为y的倍数的整数。
size_t Win32MapFile::_Roundup( size_t x, size_t y )
{
    return ((x + y - 1) / y) * y;
}

//z 返回不大于 s 的且为 _page_size 最大倍数的数字
size_t Win32MapFile::_TruncateToPageBoundary( size_t s )
{
    //z 去掉余数
    s -= (s & (_page_size - 1));
    //z 确保能够整除
    assert((s % _page_size) == 0);
    return s;
}

bool Win32MapFile::_UnmapCurrentRegion()
{
    bool result = true;
    if (_base != NULL)
    {
        //z 这意思是没有完全写满，将设置 _pending_sync 为true ？
        if (_last_sync < _limit)
        {
            // Defer syncing this data until next Sync() call, if any
            _pending_sync = true;
        }
        //z unmap file
        UnmapViewOfFile(_base);
        //z 关闭指针
        CloseHandle(_base_handle);
        //z 更新文件指针到当前的结尾部分
        _file_offset += _limit - _base;
        _base = NULL;
        _base_handle = NULL;
        _limit = NULL;
        _last_sync = NULL;
        _dst = NULL;
        // Increase the amount we map the next time, but capped at 1MB
        //z 从64k起步，每次unmap的时候会将 _map_size 增大一倍，直至到达一兆
        //z 2^6 * 2^10  ---> 2^10 * 2^10，四次之后，每次都存放一兆。
        if (_map_size < (1<<20))
        {
            _map_size *= 2;
        }
    }
    return result;
}

/*
在此函数里，会记录下 _base_file _base _limit _base _last_sync 变量
*/
bool Win32MapFile::_MapNewRegion()
{
    assert(_base == NULL);
    //LONG newSizeHigh = (LONG)((file_offset_ + map_size_) >> 32);
    //LONG newSizeLow = (LONG)((file_offset_ + map_size_) & 0xFFFFFFFF);
    //z 得到高 32 位
    DWORD off_hi = (DWORD)(_file_offset >> 32);
    //z 得到低 32 位
    DWORD off_lo = (DWORD)(_file_offset & 0xFFFFFFFF);

    LARGE_INTEGER newSize;
    newSize.QuadPart = _file_offset + _map_size;
    //z 将文件指针相对FILE_BEGIN移动到newSize处
    SetFilePointerEx(_hFile, newSize, NULL, FILE_BEGIN);
    //z 设置当前位置为文件的结束位置
    SetEndOfFile(_hFile);

    //z 创建 file mapping， 记录下 map file handle
    _base_handle = CreateFileMappingA(
                       _hFile,
                       NULL,
                       PAGE_READWRITE,
                       0,
                       0,
                       0);

    //z 映射新添加的 _map_size 这个部分
    if (_base_handle != NULL)
    {
        _base = (char*) MapViewOfFile(_base_handle,
                                      FILE_MAP_ALL_ACCESS,
                                      off_hi,
                                      off_lo,
                                      _map_size);
        if (_base != NULL)
        {
            _limit = _base + _map_size;
            _dst = _base;
            _last_sync = _base;
            return true;
        }
    }
    return false;
}

//z ctor
Win32MapFile::Win32MapFile( const std::string& fname) :
    _filename(fname),
    _hFile(NULL),
    //z page size 64k
    _page_size(Win32::g_PageSize),
    //z map size 64k
    _map_size(_Roundup(65536, Win32::g_PageSize)),
    _base(NULL),
    _base_handle(NULL),
    _limit(NULL),
    _dst(NULL),
    _last_sync(NULL),
    //z 开始时 _file_offset 为0
    _file_offset(0),
    _pending_sync(false)
{
    //z 将filename转为 wchar 的形式，打开文件，记录句柄
    _Init(Win32::MultiByteToWChar(fname.c_str() ) );
    assert((Win32::g_PageSize & (Win32::g_PageSize - 1)) == 0);
}

//z 附加数据到文件
Status Win32MapFile::Append( const Slice& data )
{
    //z 数据内容
    const char* src = data.data();
    //z 数据尺寸
    size_t left = data.size();
    Status s;
    while (left > 0)
    {
        assert(_base <= _dst);
        assert(_dst <= _limit);
        //z 查看可用空间
        size_t avail = _limit - _dst;
        //z 如果无可用空间
        if (avail == 0)
        {
            if (!_UnmapCurrentRegion() ||
                    !_MapNewRegion())
            {
                return Status::IOError("WinMmapFile.Append::UnmapCurrentRegion or MapNewRegion: ", Win32::GetLastErrSz());
            }
        }

        //z 如果 left 比可用的小，那么设置拷贝的为 left；否则设置为可用的
        size_t n = (left <= avail) ? left : avail;
        //z 可以直接向 _dst 写。
        memcpy(_dst, src, n);
        //z 移动 _dst 到新的位置
        _dst += n;
        //z 移动 src 指针到新的位置
        src += n;
        //z 拷贝 n 之后，设置新的left值。
        //z 大多数情况下，在剩余空间足够的情况下，一次就拷贝完毕了。
        left -= n;
    }
    return s;
}

Status Win32MapFile::Close()
{
    Status s;
    //z 记录下未使用的空间
    size_t unused = _limit - _dst;
    //z 然后 unmap 文件，_UnmapCurrentRegion总是返回 true
    if (!_UnmapCurrentRegion())
    {
        s = Status::IOError("WinMmapFile.Close::UnmapCurrentRegion: ",Win32::GetLastErrSz());
    }
    else if (unused > 0)
    {
        //z 从而总是调用下面这些指令
        // Trim the extra space at the end of the file
        LARGE_INTEGER newSize;
        //z 去除哪些未使用的文件部分
        newSize.QuadPart = _file_offset - unused;
        //z 设置文件指针
        if (!SetFilePointerEx(_hFile, newSize, NULL, FILE_BEGIN))
        {
            s = Status::IOError("WinMmapFile.Close::SetFilePointer: ",Win32::GetLastErrSz());
        }
        else
            //z 调用成功，设置文件的结束部分
            SetEndOfFile(_hFile);
    }

    //z 关闭文件
    if (!CloseHandle(_hFile))
    {
        if (s.ok())
        {
            s = Status::IOError("WinMmapFile.Close::CloseHandle: ", Win32::GetLastErrSz());
        }
    }

    //z 重置成员变量
    _hFile = INVALID_HANDLE_VALUE;
    _base = NULL;
    _base_handle = NULL;
    _limit = NULL;

    return s;
}

Status Win32MapFile::Sync()
{
    Status s;
    if (_pending_sync)
    {
        // Some unmapped data was not synced
        _pending_sync = false;
        if (!FlushFileBuffers(_hFile))
        {
            s = Status::IOError("WinMmapFile.Sync::FlushFileBuffers: ",Win32::GetLastErrSz());
        }
    }

    if (_dst > _last_sync)
    {
        // Find the beginnings of the pages that contain the first and last
        // bytes to be synced.
        size_t p1 = _TruncateToPageBoundary(_last_sync - _base);
        //z 事实上 _map_size 可能是 _page_size 的几倍 2^4 * 2^6 * 2^10
        size_t p2 = _TruncateToPageBoundary(_dst - _base - 1);
        //z _dst 为已写数据的截止的地方
        _last_sync = _dst;
        //z 至少写一个 _page_size
        //z 将一个 mapped view of a file 的区间的字节写到磁盘上
        if (!FlushViewOfFile(_base + p1, p2 - p1 + _page_size))
        {
            s = Status::IOError("WinMmapFile.Sync::FlushViewOfFile: ",Win32::GetLastErrSz());
        }
    }
    return s;
}

Status Win32MapFile::Flush()
{
    //z 直接返回 ok，是什么意思了？
    return Status::OK();
}

Win32MapFile::~Win32MapFile()
{
    //z 关闭文件
    if (_hFile != INVALID_HANDLE_VALUE)
    {
        Win32MapFile::Close();
    }
}

BOOL Win32MapFile::_Init( LPCWSTR Path )
{
    //z 首先查看文件是否存在。存在设置flag为打开已有的，否则总是创建。
    DWORD Flag = PathFileExistsW(Path) ? OPEN_EXISTING : CREATE_ALWAYS;
    //z 打开文件，记录文件句柄
    _hFile = CreateFileW(Path,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,
                         NULL,
                         Flag,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
    //z 检查是否创建文件成功
    if(!_hFile || _hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    else
        return TRUE;
}

BOOL Win32MapFile::isEnable()
{
    //z 查看是否打开了文件
    return _hFile ? TRUE : FALSE;
}

Win32FileLock::Win32FileLock( const std::string& fname ) :
    _hFile(NULL),_filename(fname)
{
    _Init(Win32::MultiByteToWChar(fname.c_str() ) );
}

Win32FileLock::~Win32FileLock()
{
    _CleanUp();
}

BOOL Win32FileLock::_Init( LPCWSTR path )
{
    BOOL bRet = FALSE;
    if(!_hFile)
        _hFile = ::CreateFileW(path,0,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(!_hFile || _hFile == INVALID_HANDLE_VALUE )
    {
        _hFile = NULL;
    }
    else
        bRet = TRUE;
    return bRet;
}

void Win32FileLock::_CleanUp()
{
    ::CloseHandle(_hFile);
    _hFile = NULL;
}

BOOL Win32FileLock::isEnable()
{
    return _hFile ? TRUE : FALSE;
}

Win32Logger::Win32Logger(WritableFile* pFile) : _pFileProxy(pFile)
{
    assert(_pFileProxy);
}

Win32Logger::~Win32Logger()
{
    if(_pFileProxy)
        delete _pFileProxy;
}

void Win32Logger::Logv( const char* format, va_list ap )
{
    uint64_t thread_id = ::GetCurrentThreadId();

    // We try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];
    for (int iter = 0; iter < 2; iter++)
    {
        char* base;
        int bufsize;
        if (iter == 0)
        {
            bufsize = sizeof(buffer);
            base = buffer;
        }
        else
        {
            bufsize = 30000;
            base = new char[bufsize];
        }
        char* p = base;
        char* limit = base + bufsize;

        SYSTEMTIME st;
        GetLocalTime(&st);
        p += snprintf(p, limit - p,
                      "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
                      int(st.wYear),
                      int(st.wMonth),
                      int(st.wDay),
                      int(st.wHour),
                      int(st.wMinute),
                      int(st.wMinute),
                      int(st.wMilliseconds),
                      static_cast<long long unsigned int>(thread_id));

        // Print the message
        if (p < limit)
        {
            va_list backup_ap;
            va_copy(backup_ap, ap);
            p += vsnprintf(p, limit - p, format, backup_ap);
            va_end(backup_ap);
        }

        // Truncate to available space if necessary
        if (p >= limit)
        {
            if (iter == 0)
            {
                continue;       // Try again with larger buffer
            }
            else
            {
                p = limit - 1;
            }
        }

        // Add newline if necessary
        if (p == base || p[-1] != '\n')
        {
            *p++ = '\n';
        }

        assert(p <= limit);
        DWORD hasWritten = 0;
        if(_pFileProxy)
        {
            _pFileProxy->Append(Slice(base, p - base));
            _pFileProxy->Flush();
        }
        if (base != buffer)
        {
            delete[] base;
        }
        break;
    }
}

bool Win32Env::FileExists(const std::string& fname)
{
    std::string path = fname;
    Win32::ModifyPath(path);
    return ::PathFileExistsW(Win32::MultiByteToWChar(path.c_str() ) ) ? true : false;
}

//z 得到目录下的文件，不迭代子目录
Status Win32Env::GetChildren(const std::string& dir, std::vector<std::string>* result)
{
    Status sRet;
    ::WIN32_FIND_DATAW wfd;
    std::string path = dir;
    Win32::ModifyPath(path);
    path += "\\*.*";
    ::HANDLE hFind = ::FindFirstFileW(
                         Win32::MultiByteToWChar(path.c_str() ) ,&wfd);
    if(hFind && hFind != INVALID_HANDLE_VALUE)
    {
        BOOL hasNext = TRUE;
        std::string child;
        while(hasNext)
        {
            child = Win32::WCharToMultiByte(wfd.cFileName);
            if(child != ".." && child != ".")
            {
                result->push_back(child);
            }
            hasNext = ::FindNextFileW(hFind,&wfd);
        }
        ::FindClose(hFind);
    }
    else
        sRet = Status::IOError(dir,"Could not get children.");
    return sRet;
}

void Win32Env::SleepForMicroseconds( int micros )
{
    ::Sleep((micros + 999) /1000);
}

Status Win32Env::DeleteFile( const std::string& fname )
{
    Status sRet;
    std::string path = fname;
    Win32::ModifyPath(path);
    if(!::DeleteFileW(Win32::MultiByteToWChar(path.c_str() ) ) )
    {
        sRet = Status::IOError(path, "Could not delete file.");
    }
    return sRet;
}

Status Win32Env::GetFileSize( const std::string& fname, uint64_t* file_size )
{
    Status sRet;
    std::string path = fname;
    Win32::ModifyPath(path);
    HANDLE file = ::CreateFileW(Win32::MultiByteToWChar(path.c_str()),
                                GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    LARGE_INTEGER li;
    if(::GetFileSizeEx(file,&li))
    {
        *file_size = (uint64_t)li.QuadPart;
    }
    else
        sRet = Status::IOError(path,"Could not get the file size.");
    CloseHandle(file);
    return sRet;
}

Status Win32Env::RenameFile( const std::string& src, const std::string& target )
{
    Status sRet;
    std::wstring src_path = Win32::MultiByteToWChar(src.c_str() );
    std::wstring target_path = Win32::MultiByteToWChar(target.c_str() );

    if(!MoveFile(Win32::ModifyPath(src_path).c_str(),
                 Win32::ModifyPath(target_path).c_str() ) )
    {
        DWORD err = GetLastError();
        if(err == 0x000000b7)
        {
            if(!::DeleteFileW(target_path.c_str() ) )
                sRet = Status::IOError(src, "Could not rename file.");
            else if(!::MoveFileW(Win32::ModifyPath(src_path).c_str(),
                                 Win32::ModifyPath(target_path).c_str() ) )
                sRet = Status::IOError(src, "Could not rename file.");
        }
    }
    return sRet;
}

Status Win32Env::LockFile( const std::string& fname, FileLock** lock )
{
    Status sRet;
    std::string path = fname;
    Win32::ModifyPath(path);
    Win32FileLock* _lock = new Win32FileLock(path);
    if(!_lock->isEnable())
    {
        delete _lock;
        *lock = NULL;
        sRet = Status::IOError(path, "Could not lock file.");
    }
    else
        *lock = _lock;
    return sRet;
}

Status Win32Env::UnlockFile( FileLock* lock )
{
    Status sRet;
    delete lock;
    return sRet;
}

void Win32Env::Schedule( void (*function)(void* arg), void* arg )
{
    QueueUserWorkItem(Win32::WorkItemWrapperProc,
                      new Win32::WorkItemWrapper(function,arg),
                      WT_EXECUTEDEFAULT);
}

void Win32Env::StartThread( void (*function)(void* arg), void* arg )
{
    ::_beginthread(function,0,arg);
}

Status Win32Env::GetTestDirectory( std::string* path )
{
    Status sRet;
    WCHAR TempPath[MAX_PATH];
    ::GetTempPathW(MAX_PATH,TempPath);
    *path = Win32::WCharToMultiByte(TempPath);
    path->append("leveldb\\test\\");
    Win32::ModifyPath(*path);
    return sRet;
}

uint64_t Win32Env::NowMicros()
{
#ifndef USE_VISTA_API
#define GetTickCount64 GetTickCount
#endif
    return (uint64_t)(GetTickCount64()*1000);
}

Status Win32Env::CreateDir( const std::string& dirname )
{
    Status sRet;
    std::string path = Win32::MultiByteToAnsi(dirname.c_str() );
    if(path[path.length() - 1] != '\\')
    {
        path += '\\';
    }
    Win32::ModifyPath(path);
    if(!::MakeSureDirectoryPathExists( path.c_str() ) )
    {
        sRet = Status::IOError(dirname, "Could not create directory.");
    }
    return sRet;
}

Status Win32Env::DeleteDir( const std::string& dirname )
{
    Status sRet;
    std::wstring path = Win32::MultiByteToWChar(dirname.c_str() ) ;
    Win32::ModifyPath(path);
    if(!::RemoveDirectoryW( path.c_str() ) )
    {
        sRet = Status::IOError(dirname, "Could not delete directory.");
    }
    return sRet;
}

Status Win32Env::NewSequentialFile( const std::string& fname, SequentialFile** result )
{
    Status sRet;
    std::string path = fname;
    Win32::ModifyPath(path);
    Win32SequentialFile* pFile = new Win32SequentialFile(path);
    if(pFile->isEnable())
    {
        *result = pFile;
    }
    else
    {
        delete pFile;
        sRet = Status::IOError(path, Win32::GetLastErrSz());
    }
    return sRet;
}

Status Win32Env::NewRandomAccessFile( const std::string& fname, RandomAccessFile** result )
{
    Status sRet;
    std::string path = fname;
    Win32RandomAccessFile* pFile = new Win32RandomAccessFile(Win32::ModifyPath(path));
    if(!pFile->isEnable())
    {
        delete pFile;
        *result = NULL;
        sRet = Status::IOError(path,"Could not create random access file.");
    }
    else
        *result = pFile;
    return sRet;
}

Status Win32Env::NewLogger( const std::string& fname, Logger** result )
{
    Status sRet;
    std::string path = fname;
    //z 优先使用 Win32MapFile
    Win32MapFile* pMapFile = new Win32MapFile(Win32::ModifyPath(path));
    if(!pMapFile->isEnable())
    {
        delete pMapFile;
        *result = NULL;
        sRet = Status::IOError(path,"could not create a logger.");
    }
    else
        //z 如果创建不成功，使用 Win32Logger
        *result = new Win32Logger(pMapFile);
    return sRet;
}

Status Win32Env::NewWritableFile( const std::string& fname, WritableFile** result )
{
    Status sRet;
    std::string path = fname;
    Win32MapFile* pFile = new Win32MapFile(Win32::ModifyPath(path));
    if(!pFile->isEnable())
    {
        *result = NULL;
        sRet = Status::IOError(fname,Win32::GetLastErrSz());
    }
    else
        *result = pFile;
    return sRet;
}

Win32Env::Win32Env()
{

}

Win32Env::~Win32Env()
{

}



}