//
// Created by John Stojanovski on 12/07/2016.
//

#ifndef SYSLOG_PRELAUNCHER_CUSER_H
#define SYSLOG_PRELAUNCHER_CUSER_H

#include <pwd.h>
#include <unistd.h>

class CUser {
    public:
        CUser() {};
        virtual ~CUser() {};

        static int set_id(const uid_t id) { return ::setuid(id); }

        static uid_t effective_id() { return ::geteuid(); };
        static int set_effective_id(const uid_t id) {return ::seteuid(id); };

        static int set_group_id(const gid_t id) { return ::setgid(id); }

        static int set_effective_group_id(const gid_t id) { return ::setegid(id); }

        static bool is_root() { return CUser::effective_id() == 0; };

        // TODO: these should not assume the vars exist.
        static uid_t sudo_uid() { return static_cast<uid_t>(::atoi(::getenv("SUDO_UID"))); }
        static gid_t sudo_gid() { return static_cast<gid_t>(::atoi(::getenv("SUDO_GID"))); }

        static char* id_to_user(uid_t id) {
            struct ::passwd *passwd_ent = NULL;

            passwd_ent = ::getpwuid(id);

            // TODO: need to do something better for the failure case here.
            return passwd_ent == NULL ? NULL : passwd_ent->pw_name;
        };
    };

#endif //SYSLOG_PRELAUNCHER_CUSER_H
