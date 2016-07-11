/*
 *
 * syslog_prelauncher uses the Mac OS ptrace API to elevate the syslog logging for
 * a targeted binary. Only work on mac for now
 *
 */

#include <iostream>

#include <pwd.h>
#include <unistd.h>
#include <sys/ptrace.h>

class CurrentUser {
    public:
        CurrentUser() {};
        virtual ~CurrentUser() {};

        static int set_id(uid_t id) { return ::setuid(id); }

        static uid_t effective_id() { return ::geteuid(); };
        static int set_effective_id(uid_t id) {return ::seteuid(id); };

        static int set_group_id(gid_t id) { return ::setgid(id); }

        static int set_effective_group_id(gid_t id) { return ::setegid(id); }

        static bool is_root() { return CurrentUser::effective_id() == 0; };

        // TODO: these should not assume the vars exist.
        static uid_t sudo_uid() { return static_cast<uid_t>(::atoi(::getenv("SUDO_UID"))); }
        static gid_t sudo_gid() { return static_cast<gid_t>(::atoi(::getenv("SUDO_GID"))); }

        static char* id_to_user(uid_t id) {
            struct passwd *passwd_ent = NULL;

            passwd_ent = ::getpwuid(id);

            // TODO: need to do something better for the failure case here.
            return passwd_ent == NULL ? NULL : passwd_ent->pw_name;
        };
};


typedef enum {
    NONE = 0,
    NOT_ROOT,
    FORK_FAILURE,
    CHILD_NOT_SIGTRAPPED,
    SYSLOG_FAILURE,
    WAKE_FAIL,
    TRACE_ME_FAIL
} ErrorCode;


int main(int argc, char *argv[]);
int main(int argc, char *argv[]) {
    ErrorCode error_code = ErrorCode::NONE;

    std::cout << "[-] Effective UID: " << CurrentUser::effective_id() << " (" << CurrentUser::id_to_user(CurrentUser::effective_id()) << ")." << std::endl;
    if (CurrentUser::is_root()) {
        std::cout << "[-] Actual UID: " << CurrentUser::sudo_uid() << " (" << CurrentUser::id_to_user(CurrentUser::sudo_uid()) << ")." << std::endl;
    } else {
        std::cout << "[x] Not root! Try 'sudo'." << std::endl;
        error_code = ErrorCode::NOT_ROOT;
    }

    int forked_pid = -1;
    if (error_code == ErrorCode::NONE) {
        std::cout << "[-] Forking..." << std::endl;
        forked_pid = ::fork();
        switch (forked_pid) {
            case -1: {       // Fork error.
                std::cout << "[x] ::fork() failed." << std::endl;
                error_code = ErrorCode::FORK_FAILURE;
                break;
            }

            case 0: {       // Child processes.
                std::cout << "  [-] Setting subprocess trace flag." << std::endl;
                int ptrace_result = ::ptrace(PT_TRACE_ME, 0, 0, 0);
                if (ptrace_result != 0) {
                    std::cout << "  [x] ::ptrace(PT_TRACE_ME) failed with result: " << ptrace_result << std::endl;
                    error_code = ErrorCode::TRACE_ME_FAIL;
                }

                if (error_code == ErrorCode::NONE) {
                    std::cout << "  [-] Relinquishing subprocess privileges (" <<
                    CurrentUser::id_to_user(CurrentUser::effective_id()) << ")." << std::endl;
                    CurrentUser::set_id(CurrentUser::sudo_uid());
                    CurrentUser::set_effective_id(CurrentUser::sudo_uid());
                    CurrentUser::set_group_id(CurrentUser::sudo_gid());
                    CurrentUser::set_effective_group_id(CurrentUser::sudo_gid());
                    std::cout << "  [-] New subprocess UID: " << CurrentUser::effective_id() << " (" <<
                    CurrentUser::id_to_user(CurrentUser::effective_id()) << ")." << std::endl;

                    std::cout << "  [-] Executing target..." << std::endl;
                    // TODO: This needs to come from argv.
                    ::execl("/usr/bin/touch", "touch", "/tmp/foo", NULL);
                }

                break;
            }

            default:
                break;
        }
    }

    int wstatus = 0;
    if (error_code == ErrorCode::NONE) {
        std::cout << "[-] Forked PID: " << forked_pid << std::endl;

        std::cout << "[-] Awaiting SIGTRAP..." << std::endl;
        ::wait(&wstatus);
        if (WSTOPSIG(wstatus) != SIGTRAP) {
            std::cout << "[x] Unexpected child wait status received!" << std::endl;
            error_code = ErrorCode::CHILD_NOT_SIGTRAPPED;
        }
    }

    if (error_code == ErrorCode::NONE) {
        std::cout << "[-] Received SIGTRAP." << std::endl;

        std::cout << "[-] Reconfiguring syslog for PID: " << forked_pid << std::endl;
        std::string syslog_pid_command = "/usr/bin/syslog -c ";
        syslog_pid_command += std::to_string(forked_pid);
        syslog_pid_command += " -d";

        int syslog_status = ::system(syslog_pid_command.c_str());
        if (syslog_status != 0) {
            std::cout << "[x] Syslog failed with status: " << syslog_status << syslog_status << std::endl;
            error_code = ErrorCode::SYSLOG_FAILURE;
        }
    }

    if (error_code == ErrorCode::NONE) {
        std::cout << "[-] Waking PID: " << forked_pid << std::endl;
        int ptrace_result = ::ptrace(PT_CONTINUE, forked_pid, (caddr_t) 1, 0);
        if (ptrace_result != 0) {
            std::cout << "[x] Failed waking PID; ::ptrace(PT_CONTINUE) result: " << ptrace_result << std::endl;
            error_code = ErrorCode::WAKE_FAIL;
        }
    }

    if (error_code == ErrorCode::NONE) {
        std::cout << "[-] Waiting for subprocess to complete..." << std::endl;
        ::wait(&wstatus);
    }

    std::cout << (error_code == ErrorCode::NONE ? "[-] " : "[x] ") << "Done." << std::endl;

    return error_code;
}