#include "../gcode_parser.h"
#include "../gcode_interpreter.h"

#include <stdio.h>
#include <locale.h>

typedef struct TrivialDict {
    const char* key;
    GCodeVal* value;
} TrivialDict;

typedef struct CLI {
    FILE* input;
    GCodeParser* parser;
    GCodeInterpreter* interp;
} CLI;

void error(void* context, const GCodeError* error) {
    printf("*** ERROR: %s\n", gcode_error_get(error));
}

bool statement(void* context, GCodeStatementNode* statement) {
    gcode_interp_exec(((CLI*)context)->interp, statement);
    gcode_node_delete((GCodeNode*)statement);
    return true;
}

bool lookup(void* context, const GCodeVal* key, dict_handle_t parent,
            GCodeVal* result)
{
    // Trivial "dictionary" semantics, a simple dictionary consisting of
    // { FOO: { BAR: { BIZ: "baz" } } }
    CLI* cli = (CLI*)context;
    if (!parent) {
        if (!strcmp(gcode_str_cast(cli->interp, key), "FOO")) {
            result->type = GCODE_VAL_DICT;
            result->dict_val = "foo";
        }
    } else if (!strcmp((const char*)parent, "foo")) {
        if (!strcmp(gcode_str_cast(cli->interp, key), "BAR")) {
            result->type = GCODE_VAL_DICT;
            result->dict_val = "bar";
        }
    } else if (!strcmp((const char*)parent, "bar")) {
        if (!strcmp(gcode_str_cast(cli->interp, key), "BIZ")) {
            result->type = GCODE_VAL_STR;
            result->str_val = "baz";
        }
    }
    return true;
}

const char* serialize(void* context, dict_handle_t dict) {
    if (!strcmp((const char*)dict, "foo"))
        return "#<dict:foo>";
    if (!strcmp((const char*)dict, "bar"))
        return "#<dict:bar>";
    return "";
}

bool exec(void* context, const char* command, const char** args, size_t count) {
    puts(command);
    for (size_t i = 0; i < count; i++) {
        putchar(' ');
        puts(args[i]);
    }
    putchar('\n');
    return true;
}

void cli_delete(CLI* cli) {
    if (cli->input)
        fclose(cli->input);
    gcode_parser_delete(cli->parser);
    gcode_interp_delete(cli->interp);
    free(cli);
}

CLI* cli_new(const char* input_filename) {
    CLI* cli = malloc(sizeof(CLI));
    if (!cli) {
        fprintf(stderr, "Out of memory allocating CLI");
        return NULL;
    }
    memset(cli, 0, sizeof(CLI));

    cli->input = fopen(input_filename, "r");
    if (!cli->input) {
        fprintf(stderr, "Error opening input file");
        cli_delete(cli);
        return NULL;
    }

    cli->parser = gcode_parser_new(cli, error, statement);
    if (!cli->parser) {
        fprintf(stderr, "Out of memory (parser creation)");
        cli_delete(cli);
        return NULL;
    }

    cli->interp = gcode_interp_new(cli, error, lookup, serialize, exec);
    if (!cli->interp) {
        fprintf(stderr, "Out of memory (interpreter creation)");
        cli_delete(cli);
        return NULL;
    }

    return cli;
}

void cli_run(CLI* cli) {
    char buf[4096];

    while(!feof(cli->input)) {
        size_t length = fread(buf, 1, 4096, cli->input);
        if (ferror(cli->input)) {
            fprintf(stderr, "I/O error reading input");
            return;
        }
        gcode_parser_parse(cli->parser, buf, length);
    }

    gcode_parser_finish(cli->parser);
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILENAME\n", argv[0]);
        return 1;
    }

    setlocale(LC_ALL, "");

    CLI* cli = cli_new(argv[1]);
    if (!cli)
        return 1;

    cli_run(cli);

    cli_delete(cli);

    return 0;
}
