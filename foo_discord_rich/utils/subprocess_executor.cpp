#include <stdafx.h>

#include "subprocess_executor.h"

#include <qwr/abort_callback.h>
#include <qwr/winapi_error_helpers.h>

#include <limits>
#include <vector>

namespace
{

constexpr size_t kMaxCapturedOutputBytes = 64 * 1024;

} // namespace

namespace drp
{

SubprocessExecutor::SubprocessExecutor( const qwr::u8string& command )
{
    try
    {
        CreatePipes();
        CreateProcess( command );
        CreateJob();
    }
    catch ( ... )
    {
        TerminateProcessTree();
        throw;
    }
}

SubprocessExecutor::~SubprocessExecutor()
{
    TerminateProcessTree();
}

void SubprocessExecutor::Start()
{
    qwr::QwrException::ExpectTrue( handles_.hThread.get(), "SubprocessExecutor error: null hThread" );

    auto iRet = ::ResumeThread( handles_.hThread.get() );
    qwr::error::CheckWinApi( iRet != -1, "ResumeThread" );
}

void SubprocessExecutor::WriteData( const qwr::u8string& data )
{
    qwr::QwrException::ExpectTrue( handles_.hStdinWrite.get(), "SubprocessExecutor error: null hStdinWrite" );

    size_t writeOffset = 0;
    while ( writeOffset < data.size() )
    {
        DWORD bytesWritten = 0;
        const auto bytesToWrite = static_cast<DWORD>( std::min<size_t>( data.size() - writeOffset, std::numeric_limits<DWORD>::max() ) );
        const auto bRet = ::WriteFile(
            handles_.hStdinWrite.get(),
            data.data() + writeOffset,
            bytesToWrite,
            &bytesWritten,
            nullptr );
        qwr::error::CheckWinApi( bRet, "WriteFile" );
        writeOffset += bytesWritten;
        if ( bytesWritten == 0 )
        {
            break;
        }
    }
    qwr::QwrException::ExpectTrue( writeOffset == data.size(), "SubprocessExecutor error: failed to write full data to stdin" );

    handles_.hStdinWrite.reset();
}

DWORD SubprocessExecutor::WaitUntilCompleted( const std::chrono::seconds& timeout )
{
    return WaitUntilCompleted( timeout, qwr::GlobalAbortCallback::GetInstance() );
}

DWORD SubprocessExecutor::WaitUntilCompleted( const std::chrono::seconds& timeout, abort_callback& aborter )
{
    qwr::QwrException::ExpectTrue( handles_.hProcess.get(), "SubprocessExecutor error: null hProcess" );
    qwr::QwrException::ExpectTrue( handles_.hThread.get(), "SubprocessExecutor error: null hThread" );
    qwr::QwrException::ExpectTrue( handles_.hStdinRead.get(), "SubprocessExecutor error: null hStdinRead" );
    qwr::QwrException::ExpectTrue( handles_.hStdoutWrite.get(), "SubprocessExecutor error: null hStdoutWrite" );
    qwr::QwrException::ExpectTrue( handles_.hStderrWrite.get(), "SubprocessExecutor error: null hStderrWrite" );

    qwr::TimedAbortCallback timeoutAborter{ "", timeout };

    handles_.hStdoutWrite.reset();
    handles_.hStderrWrite.reset();
    handles_.hStdinRead.reset();

    std::array handlesToWait{ handles_.hProcess.get(), aborter.get_handle(), timeoutAborter.get_handle() };
    bool isProcessCompleted = false;
    while ( !isProcessCompleted )
    {
        ReadAvailableDataFromPipe( handles_.hStdoutRead.get(), output_ );
        ReadAvailableDataFromPipe( handles_.hStderrRead.get(), errorOutput_ );

        const auto waitResult = WaitForMultipleObjects( static_cast<DWORD>( handlesToWait.size() ), handlesToWait.data(), false, 50 );
        if ( waitResult == WAIT_OBJECT_0 )
        {
            isProcessCompleted = true;
        }
        else if ( waitResult == WAIT_TIMEOUT )
        {
            continue;
        }
        else if ( waitResult == WAIT_OBJECT_0 + 1 )
        {
            aborter.check();
        }
        else if ( waitResult == WAIT_OBJECT_0 + 2 )
        {
            timeoutAborter.check();
        }
        else
        {
            qwr::error::CheckWinApi( false, "WaitForMultipleObjects" );
        }
    }

    ReadAvailableDataFromPipe( handles_.hStdoutRead.get(), output_ );
    ReadAvailableDataFromPipe( handles_.hStderrRead.get(), errorOutput_ );

    DWORD exitCode = 0;
    auto bRet = ::GetExitCodeProcess( handles_.hProcess.get(), &exitCode );
    qwr::error::CheckWinApi( bRet, "GetExitCodeProcess" );

    handles_.hProcess.reset();
    handles_.hThread.reset();

    return exitCode;
}

std::optional<qwr::u8string> SubprocessExecutor::GetOutput()
{
    qwr::QwrException::ExpectTrue( handles_.hStdoutRead.get(), "SubprocessExecutor error: null hStdoutRead" );

    return TrimOutput( output_ );
}

std::optional<qwr::u8string> SubprocessExecutor::GetErrorOutput()
{
    qwr::QwrException::ExpectTrue( handles_.hStderrRead.get(), "SubprocessExecutor error: null hStderrRead" );

    return TrimOutput( errorOutput_ );
}

void SubprocessExecutor::CreatePipes()
{
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof( SECURITY_ATTRIBUTES );
    saAttr.bInheritHandle = true;
    saAttr.lpSecurityDescriptor = nullptr;

    {
        HANDLE hRead{};
        HANDLE hWrite{};
        auto bRet = ::CreatePipe( &hRead, &hWrite, &saAttr, 0 );
        qwr::error::CheckWinApi( bRet, "CreatePipe(stdin)" );

        handles_.hStdinRead.reset( hRead );
        handles_.hStdinWrite.reset( hWrite );

        bRet = SetHandleInformation( hWrite, HANDLE_FLAG_INHERIT, 0 );
        qwr::error::CheckWinApi( bRet, "SetHandleInformation(stdin)" );
    }

    {
        HANDLE hRead{};
        HANDLE hWrite{};
        auto bRet = ::CreatePipe( &hRead, &hWrite, &saAttr, 0 );
        qwr::error::CheckWinApi( bRet, "CreatePipe(stdout)" );

        handles_.hStdoutRead.reset( hRead );
        handles_.hStdoutWrite.reset( hWrite );

        bRet = SetHandleInformation( hRead, HANDLE_FLAG_INHERIT, 0 );
        qwr::error::CheckWinApi( bRet, "SetHandleInformation(stdout)" );
    }

    {
        HANDLE hRead{};
        HANDLE hWrite{};
        auto bRet = ::CreatePipe( &hRead, &hWrite, &saAttr, 0 );
        qwr::error::CheckWinApi( bRet, "CreatePipe(stderr)" );

        handles_.hStderrRead.reset( hRead );
        handles_.hStderrWrite.reset( hWrite );

        bRet = SetHandleInformation( hRead, HANDLE_FLAG_INHERIT, 0 );
        qwr::error::CheckWinApi( bRet, "SetHandleInformation(stderr)" );
    }
}

void SubprocessExecutor::CreateProcess( const qwr::u8string& command )
{
    auto commandW = qwr::unicode::ToWide( command );
    PROCESS_INFORMATION pi{};
    STARTUPINFO si{};
    si.cb = sizeof( si );
    si.hStdInput = handles_.hStdinRead.get();
    si.hStdOutput = handles_.hStdoutWrite.get();
    si.hStdError = handles_.hStderrWrite.get();
    si.dwFlags |= STARTF_USESTDHANDLES;
    auto bRet = ::CreateProcess( nullptr,
                                 commandW.data(),
                                 nullptr,
                                 nullptr,
                                 true,
                                 CREATE_SUSPENDED | CREATE_NO_WINDOW,
                                 nullptr,
                                 nullptr,
                                 &si,
                                 &pi );
    qwr::error::CheckWinApi( bRet, "CreateProcess" );

    handles_.hProcess.reset( pi.hProcess );
    handles_.hThread.reset( pi.hThread );
}

void SubprocessExecutor::CreateJob()
{
    const auto hJob = ::CreateJobObject( nullptr, nullptr );
    qwr::error::CheckWinApi( hJob, "CreateJobObject" );

    handles_.hJob.reset( hJob );

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    auto bRet = ::SetInformationJobObject( hJob, JobObjectExtendedLimitInformation, &jeli, sizeof( jeli ) );
    qwr::error::CheckWinApi( bRet, "SetInformationJobObject" );

    bRet = ::AssignProcessToJobObject( hJob, handles_.hProcess.get() );
    qwr::error::CheckWinApi( bRet, "AssignProcessToJobObject" );
}

void SubprocessExecutor::TerminateProcessTree() noexcept
{
    if ( handles_.hProcess )
    {
        if ( handles_.hJob )
        {
            ::TerminateJobObject( handles_.hJob.get(), 1 );
        }
        else
        {
            ::TerminateProcess( handles_.hProcess.get(), 1 );
        }
    }
}

std::optional<qwr::u8string> SubprocessExecutor::ReadDataFromPipe( HANDLE hPipe )
{
    qwr::u8string output;
    ReadAvailableDataFromPipe( hPipe, output );
    return TrimOutput( output );
}

void SubprocessExecutor::ReadAvailableDataFromPipe( HANDLE hPipe, qwr::u8string& output )
{
    if ( !hPipe )
    {
        return;
    }

    DWORD availableBytes = 0;
    while ( ::PeekNamedPipe( hPipe, nullptr, 0, nullptr, &availableBytes, nullptr ) && availableBytes )
    {
        std::vector<char> buf( std::min<DWORD>( availableBytes, 4096 ) );
        DWORD bytesRead = 0;
        const auto bRet = ::ReadFile( hPipe, buf.data(), static_cast<DWORD>( buf.size() ), &bytesRead, nullptr );
        if ( !bRet )
        {
            if ( GetLastError() != ERROR_BROKEN_PIPE )
            {
                qwr::error::CheckWinApi( false, "ReadFile" );
            }
            break;
        }

        if ( bytesRead == 0 )
        {
            break;
        }

        if ( output.size() < kMaxCapturedOutputBytes )
        {
            const auto remainingCapacity = kMaxCapturedOutputBytes - output.size();
            output.append( buf.data(), std::min<size_t>( bytesRead, remainingCapacity ) );
        }
        availableBytes = 0;
    }
}

std::optional<qwr::u8string> SubprocessExecutor::TrimOutput( const qwr::u8string& output )
{
    qwr::u8string_view trimmedOutput{ output };
    const auto lastNotWhitespace = trimmedOutput.find_last_not_of( " \t\n\r" );
    if ( lastNotWhitespace == qwr::u8string_view::npos )
    {
        return std::nullopt;
    }

    trimmedOutput = trimmedOutput.substr( 0, lastNotWhitespace + 1 );
    if ( trimmedOutput.empty() )
    {
        return std::nullopt;
    }

    return qwr::u8string{ trimmedOutput.data(), trimmedOutput.size() };
}

} // namespace drp
