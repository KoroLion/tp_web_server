//
// Created by KoroLion on 12/7/2021.
//

#ifndef WEB_SERVER_WSGI_H
#define WEB_SERVER_WSGI_H

#include "stdlib.h"
#include "stdio.h"

#define WSGI_PATH "/home/korolion/liokor_engine/"
#define PYTHON_PATH "/home/korolion/miniconda3/bin/python"

const char *script = "import io\n"
                     "from liokor_engine.wsgi import application\n"
                     "from http_parser import parse_request\n"
                     "\n"
                     "request = '''%s'''\n"
                     "request = parse_request(request)\n"
                     "\n"
                     "def start_response(status, headers):\n"
                     "        print('HTTP/1.1 {}'.format(status))\n"
                     "        for header in headers:\n"
                     "                print('{}: {}'.format(header[0], header[1]))\n"
                     "\n"
                     "environ = {\n"
                     "    'REQUEST_METHOD': request['method'],\n"
                     "    'PATH_INFO': request['path'],\n"
                     "    'SERVER_NAME': 'temp.liokor.com',\n"
                     "    'SERVER_PORT': '80',\n"
                     "\n"
                     "    'wsgi.version': (1, 0),\n"
                     "    'wsgi.input': io.StringIO(request['body']),\n"
                     "}\n"
                     "\n"
                     "for header in request['headers']:\n"
                     "    name = 'HTTP_{}'.format(header[0].upper())\n"
                     "    if name == 'HTTP_CONTENT-TYPE':\n"
                     "        environ['CONTENT_TYPE'] = header[1]\n"
                     "    if name == 'HTTP_CONTENT-LENGTH':\n"
                     "        environ['CONTENT_LENGTH'] = header[1]\n"
                     "    environ[name] = header[1]\n"
                     "\n"
                     "template = application(environ, start_response)\n"
                     "print(template.content.decode())"

char *rand_string() {
    int len = 32;
    char *str = malloc(len + 1);
    for (int i = 0; i < len; i++) {
        str[i] = 48 + rand() % 10;
    }
    str[len] = 0;
    return str;
}

char* get_wsgi_answer() {
    char *fname = rand_string();
    char fpath[2048];
    snprintf(fpath, 2048, "%s%s", WSGI_PATH, fname);
    free(fname);

    FILE *script_fp = fopen(fpath, "w");
    fputs(script, script_fp);
    fclose(script_fp);

    char command[2248];
    sprintf(command, "cd %s && %s %s", WSGI_PATH, PYTHON_PATH, fpath);

    FILE *fp = popen(command, "r");

    char buffer[4096];

    char *answer = malloc(4096 * 20);
    answer[0] = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncat(answer, buffer, 4096);
    }

    pclose(fp);

    remove(fpath);

    return answer;
}

#endif //WEB_SERVER_WSGI_H
