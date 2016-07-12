//
// Created by John Stojanovski on 12/07/2016.
//

#ifndef SYSLOG_PRELAUNCHER_CCMDLINE_H
#define SYSLOG_PRELAUNCHER_CCMDLINE_H

#include <iostream>
#include <vector>

#include <libgen.h>

#include "CFile.h"

class CCmdLine {
    public:
        CCmdLine() {};
        virtual ~CCmdLine() {};

        void parse(int argc, char *argv[]) {
            m_app = ::basename(argv[0]);

            for (int i = 1; i < argc; i++) {
                if (i != 1) {
                    m_app_arguments += " ";
                }

                m_app_arguments += (argv[i]);
                m_vectored_app_arguments.push_back(argv[i]);

                if (i == 1) {
                    m_target_path = argv[i];
                    CFile::basename(m_target_path, m_target);
                }

                if (i > 1) {
                    if (i != 2) {
                        m_target_arguments += " ";
                    }
                    m_target_arguments += argv[i];
                }
            }
        }

        std::string                 m_app;
        std::string                 m_app_arguments;
        std::vector<std::string>    m_vectored_app_arguments;

        std::string                 m_target_path;
        std::string                 m_target;
        std::string                 m_target_arguments;
};

#endif //SYSLOG_PRELAUNCHER_CCMDLINE_H
