/*
 *
 * syslog_prelauncher uses the Mac OS ptrace API to elevate the syslog logging for
 * a targeted binary. Only work on mac for now
 *
 */

#include <iostream>

#include <sys/ptrace.h>


#include "CUser.h"
#include "CFile.h"
#include "CCmdLine.h"

typedef enum {
    ERROR_NONE = 0,
    ERROR_NOT_ROOT,
    ERROR_CMD_LINE,
    ERROR_FILE_NOT_FOUND,
    ERROR_FILE_NOT_EXECUTABLE,
    ERROR_FORK_FAILURE,
    ERROR_CHILD_NOT_SIGTRAPPED,
    ERROR_SYSLOG_FAILURE,
    ERROR_WAKE_FAIL,
    ERROR_TRACE_ME_FAIL
} Error;


void usage(std::string& appname);
void usage(std::string& appname) {
    std::cout << "Usage: [sudo] " << appname << " /path/target_executable <target_params>" << std::endl;
    std::cout << "  e.g. sudo " << appname << " /usr/bin/touch /tmp/foo" << std::endl << std::endl;
}

int main(int argc, char *argv[]);
int main(int argc, char *argv[]) {
    Error error = Error::ERROR_NONE;

    std::cout << "[-] Effective UID: " << CUser::effective_id() << " (" << CUser::id_to_user(CUser::effective_id()) << ")." << std::endl;
    if (CUser::is_root()) {
        std::cout << "[-] Actual UID: " << CUser::sudo_uid() << " (" << CUser::id_to_user(CUser::sudo_uid()) << ")." << std::endl;
    } else {
        std::cout << "[x] Not root! Try 'sudo'." << std::endl;
        error = Error::ERROR_NOT_ROOT;
    }

    CCmdLine command_line;
    if (error == Error::ERROR_NONE) {
        command_line.parse(argc, argv);
        if (command_line.m_vectored_app_arguments.size() == 0) {
            std::cout << "[x] Not enough arguments!" << std::endl << std::endl;
            usage(command_line.m_app);
            error = Error::ERROR_CMD_LINE;
        }
    }

    if (error == Error::ERROR_NONE) {
        if (! CFile::exists(command_line.m_vectored_app_arguments[0].c_str())) {
            std::cout << "[x] Can't find target '" << command_line.m_vectored_app_arguments[0] << "'; Specify full path?" << std::endl;
            error = Error::ERROR_FILE_NOT_FOUND;
        }
    }

    if (error == Error::ERROR_NONE) {
        if (! CFile::is_executable(command_line.m_vectored_app_arguments[0].c_str())) {
            std::cout << "[x] Target file '" << command_line.m_vectored_app_arguments[0] << "' appears to not be executable?" << std::endl;
            error = Error::ERROR_FILE_NOT_EXECUTABLE;
        }
    }

    int forked_pid = -1;
    if (error == Error::ERROR_NONE) {
        std::cout << "[-] Forking..." << std::endl;
        forked_pid = ::fork();
        switch (forked_pid) {
            case -1: {       // Fork error.
                std::cout << "[x] ::fork() failed." << std::endl;
                error = Error::ERROR_FORK_FAILURE;
                break;
            }

            case 0: {       // Child processes.
                std::cout << "  [-] Setting subprocess trace flag." << std::endl;
                int ptrace_result = ::ptrace(PT_TRACE_ME, 0, 0, 0);
                if (ptrace_result != 0) {
                    std::cout << "  [x] ::ptrace(PT_TRACE_ME) failed with result: " << ptrace_result << std::endl;
                    error = Error::ERROR_TRACE_ME_FAIL;
                }

                if (error == Error::ERROR_NONE) {
                    std::cout << "  [-] Relinquishing subprocess privileges (" <<
                    CUser::id_to_user(CUser::effective_id()) << ")." << std::endl;
                    CUser::set_id(CUser::sudo_uid());
                    CUser::set_effective_id(CUser::sudo_uid());
                    CUser::set_group_id(CUser::sudo_gid());
                    CUser::set_effective_group_id(CUser::sudo_gid());
                    std::cout << "  [-] New subprocess UID: " << CUser::effective_id() << " (" <<
                    CUser::id_to_user(CUser::effective_id()) << ")." << std::endl;

                    std::cout << "  [-] Executing target: " << command_line.m_target_path.c_str() << " " << command_line.m_target_arguments.c_str() << std::endl;
                    ::execl(command_line.m_target_path.c_str(), command_line.m_target.c_str(), command_line.m_target_arguments.c_str(), NULL);
                }

                break;
            }

            default:
                break;
        }
    }

    int wstatus = 0;
    if (error == Error::ERROR_NONE) {
        std::cout << "[-] Forked PID: " << forked_pid << std::endl;

        std::cout << "[-] Awaiting SIGTRAP..." << std::endl;
        ::wait(&wstatus);
        if (WSTOPSIG(wstatus) != SIGTRAP) {
            std::cout << "[x] Unexpected child wait status received!" << std::endl;
            error = Error::ERROR_CHILD_NOT_SIGTRAPPED;
        }
    }

    if (error == Error::ERROR_NONE) {
        std::cout << "[-] Received SIGTRAP." << std::endl;

        std::cout << "[-] Reconfiguring syslog for PID: " << forked_pid << std::endl;
        std::string syslog_pid_command = "/usr/bin/syslog -c ";
        syslog_pid_command += std::to_string(forked_pid);
        syslog_pid_command += " -d";

        int syslog_status = ::system(syslog_pid_command.c_str());
        if (syslog_status != 0) {
            std::cout << "[x] Syslog failed with status: " << syslog_status << syslog_status << std::endl;
            error = Error::ERROR_SYSLOG_FAILURE;
        }
    }

    if (error == Error::ERROR_NONE) {
        std::cout << "[-] Waking PID: " << forked_pid << std::endl;
        int ptrace_result = ::ptrace(PT_CONTINUE, forked_pid, (caddr_t) 1, 0);
        if (ptrace_result != 0) {
            std::cout << "[x] Failed waking PID; ::ptrace(PT_CONTINUE) result: " << ptrace_result << std::endl;
            error = Error::ERROR_WAKE_FAIL;
        }
    }

    if (error == Error::ERROR_NONE) {
        std::cout << "[-] Waiting for the subprocess to complete..." << std::endl;
        ::wait(&wstatus);
    }

    std::cout << (error == Error::ERROR_NONE ? "[-] " : "[x] ") << "Done." << std::endl;

    return error;
}