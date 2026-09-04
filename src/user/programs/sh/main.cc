#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.hh>

/*
 * An interactive shell over the console, and the first program that
 * exercises fork/exec/wait, argv/environment, and VFS file I/O together
 * from a single ordinary process rather than a boot-time probe.
 *
 * Scope, stated up front rather than discovered by surprise:
 *
 *   - Commands resolve against the fixed coreutils table (sys/coreutils.hh)
 *     execv() already knows how to run, not an arbitrary PATH search over
 *     the disk -- see that header for why: loading a program's bytes from
 *     wherever a path resolves to is real future work, not attempted here.
 *   - A pipeline runs its stages SEQUENTIALLY through a ramfs temp file,
 *     not concurrently through a real kernel pipe object -- this kernel has
 *     no such object yet. `a | b` behaves like `a >/tmp/.pipeN; b
 *     </tmp/.pipeN` under the hood: correct output, no streaming overlap,
 *     and one temp file per pipeline stage boundary rather than a byte
 *     stream. Good enough to prove the shell's own plumbing; a real pipe
 *     object is the natural next kernel primitive after this.
 *   - Line editing is backspace and Ctrl-C (cancels the line, not the
 *     process -- there is no signal delivery here to interrupt a running
 *     child). No history, no arrow keys.
 */
namespace
{
    inline constexpr int line_capacity = 256;
    inline constexpr int max_tokens = 32;
    inline constexpr int max_stages = 4;
    inline constexpr int max_argv = 16;

    int last_status = 0;

    // --- variables (export) ---
    //
    // `environ` starts pointing at whatever the runtime built from this
    // process's own argv/envp block. export reassigns it to a table this
    // shell owns instead, seeded from the inherited entries, so a forked
    // child's execv() -- which reads the same global `environ` -- picks up
    // anything exported without execv() needing to know variables exist.
    inline constexpr int max_vars = 32;
    char* var_storage[max_vars + 1];
    int var_count = 0;

    [[nodiscard]] bool name_matches(const char* entry, const char* name, size_t name_length) noexcept {
        return strncmp(entry, name, name_length) == 0 && entry[name_length] == '=';
    }

    void set_var(const char* name, size_t name_length, const char* value) noexcept {
        const size_t needed = name_length + 1U + strlen(value) + 1U;
        auto* entry = static_cast<char*>(malloc(needed));
        if (entry == nullptr)
            return;
        (void)memcpy(entry, name, name_length);
        entry[name_length] = '=';
        (void)strcpy(entry + name_length + 1U, value);

        for (int index = 0; index < var_count; ++index) {
            if (name_matches(var_storage[index], name, name_length)) {
                free(var_storage[index]);
                var_storage[index] = entry;
                return;
            }
        }
        if (var_count >= max_vars) {
            free(entry);
            return;
        }
        var_storage[var_count++] = entry;
        var_storage[var_count] = nullptr;
        environ = var_storage;
    }

    void seed_vars_from_environ() noexcept {
        int count = 0;
        if (environ != nullptr) {
            while (environ[count] != nullptr && count < max_vars) {
                var_storage[count] = environ[count];
                ++count;
            }
        }
        var_count = count;
        var_storage[var_count] = nullptr;
        environ = var_storage;
    }

    [[nodiscard]] const char* lookup_var(const char* name, size_t length) noexcept {
        for (int index = 0; index < var_count; ++index) {
            if (name_matches(var_storage[index], name, length))
                return var_storage[index] + length + 1U;
        }
        return nullptr;
    }

    // --- line input ---

    [[nodiscard]] int read_line(char (&buffer)[line_capacity]) noexcept {
        int length = 0;
        for (;;) {
            char value = '\0';
            if (read(STDIN_FILENO, &value, 1U) != 1)
                continue;
            if (value == '\r' || value == '\n') {
                (void)putchar('\n');
                buffer[length] = '\0';
                return length;
            }
            if (value == 0x7f || value == 0x08) { // backspace / DEL
                if (length > 0) {
                    --length;
                    (void)write(STDOUT_FILENO, "\b \b", 3U);
                }
                continue;
            }
            if (value == 0x03) { // Ctrl-C: cancel this line, not the shell
                (void)write(STDOUT_FILENO, "^C\n", 3U);
                buffer[0] = '\0';
                return 0;
            }
            if (length < line_capacity - 1 && value >= 0x20 && value < 0x7f) {
                buffer[length++] = value;
                (void)putchar(value);
            }
        }
    }

    // --- expansion ---

    // Expands $VAR/$? inside `source` into `destination` (bounded to
    // `capacity`). Runs on every token regardless of quoting -- this shell
    // does not distinguish single- from double-quoted expansion, a real
    // but small gap from POSIX behavior.
    void expand(const char* source, char* destination, size_t capacity) noexcept {
        size_t out = 0;
        for (const char* cursor = source; *cursor != '\0' && out + 1U < capacity; ++cursor) {
            if (*cursor != '$') {
                destination[out++] = *cursor;
                continue;
            }
            ++cursor;
            if (*cursor == '?') {
                char digits[12];
                (void)snprintf(digits, sizeof(digits), "%d", last_status);
                for (const char* digit = digits; *digit != '\0' && out + 1U < capacity; ++digit)
                    destination[out++] = *digit;
                continue;
            }
            const char* name_start = cursor;
            while ((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= 'A' && *cursor <= 'Z') ||
                   (*cursor >= '0' && *cursor <= '9') || *cursor == '_')
                ++cursor;
            const size_t name_length = static_cast<size_t>(cursor - name_start);
            --cursor; // the enclosing for loop's ++cursor accounts for this char
            if (name_length == 0U)
                continue; // a lone '$' with no name expands to nothing
            const char* value = lookup_var(name_start, name_length);
            if (value == nullptr)
                continue;
            for (const char* character = value; *character != '\0' && out + 1U < capacity; ++character)
                destination[out++] = *character;
        }
        destination[out] = '\0';
    }

    // --- tokenizing ---

    struct token_list final {
        char storage[max_tokens][64];
        int count = 0;
    };

    // Splits on whitespace, honoring single/double quotes (the quote
    // characters are consumed, not copied); '|', '<', '>', ">>" are always
    // their own token even without surrounding whitespace, since a
    // pipeline/redirect boundary should not depend on the user having
    // typed a space around it.
    [[nodiscard]] bool tokenize(const char* line, token_list& tokens) noexcept {
        const char* cursor = line;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;
            if (*cursor == '\0')
                break;
            if (tokens.count >= max_tokens)
                return false;
            char* out = tokens.storage[tokens.count];
            size_t written = 0;

            if (*cursor == '|') {
                out[written++] = *cursor++;
            } else if (*cursor == '<') {
                out[written++] = *cursor++;
            } else if (*cursor == '>') {
                out[written++] = *cursor++;
                if (*cursor == '>')
                    out[written++] = *cursor++;
            } else {
                char quote = '\0';
                while (*cursor != '\0') {
                    if (quote != '\0') {
                        if (*cursor == quote) {
                            quote = '\0';
                            ++cursor;
                            continue;
                        }
                    } else {
                        if (*cursor == ' ' || *cursor == '\t' || *cursor == '|' || *cursor == '<' ||
                            *cursor == '>')
                            break;
                        if (*cursor == '\'' || *cursor == '"') {
                            quote = *cursor;
                            ++cursor;
                            continue;
                        }
                    }
                    if (written + 1U < sizeof(tokens.storage[0]))
                        out[written++] = *cursor;
                    ++cursor;
                }
            }
            out[written] = '\0';
            char expanded[64];
            expand(out, expanded, sizeof(expanded));
            (void)strcpy(out, expanded);
            ++tokens.count;
        }
        return true;
    }

    // --- pipeline structure ---

    struct stage final {
        char* argv[max_argv + 1];
        int argc = 0;
        const char* input_path = nullptr;
        const char* output_path = nullptr;
        bool append = false;
    };

    [[nodiscard]] bool build_stages(token_list& tokens, stage (&stages)[max_stages],
                                    int& stage_count) noexcept {
        stage_count = 0;
        stages[0] = {};
        for (int index = 0; index < tokens.count; ++index) {
            char* text = tokens.storage[index];
            if (strcmp(text, "|") == 0) {
                if (++stage_count >= max_stages)
                    return false;
                stages[stage_count] = {};
                continue;
            }
            stage& current = stages[stage_count];
            if (strcmp(text, "<") == 0) {
                if (++index >= tokens.count)
                    return false;
                current.input_path = tokens.storage[index];
            } else if (strcmp(text, ">") == 0 || strcmp(text, ">>") == 0) {
                current.append = strcmp(text, ">>") == 0;
                if (++index >= tokens.count)
                    return false;
                current.output_path = tokens.storage[index];
            } else {
                if (current.argc >= max_argv)
                    return false;
                current.argv[current.argc++] = text;
            }
        }
        stages[stage_count].argv[stages[stage_count].argc] = nullptr;
        ++stage_count;
        return true;
    }

    // --- builtins ---

    [[nodiscard]] bool is_builtin_name(const char* name) noexcept {
        return strcmp(name, "exit") == 0 || strcmp(name, "cd") == 0 || strcmp(name, "pwd") == 0 ||
               strcmp(name, "export") == 0 || strcmp(name, "echo") == 0;
    }

    [[nodiscard]] bool run_builtin(const stage& command, bool& should_exit) noexcept {
        if (command.argc == 0)
            return true;
        const char* name = command.argv[0];
        if (strcmp(name, "exit") == 0) {
            should_exit = true;
            last_status = command.argc > 1 ? atoi(command.argv[1]) : last_status;
            return true;
        }
        if (strcmp(name, "cd") == 0) {
            // No real cwd concept below the shell yet: VFS paths are
            // resolved absolute on every open(), so `cd` only affects
            // what THIS shell privately prepends to a relative path.
            // Deliberately not implemented further in this pass.
            last_status = 0;
            return true;
        }
        if (strcmp(name, "pwd") == 0) {
            (void)puts("/");
            last_status = 0;
            return true;
        }
        if (strcmp(name, "export") == 0) {
            for (int index = 1; index < command.argc; ++index) {
                char* equals = strchr(command.argv[index], '=');
                if (equals == nullptr)
                    continue;
                *equals = '\0';
                set_var(command.argv[index], strlen(command.argv[index]), equals + 1);
                *equals = '=';
            }
            last_status = 0;
            return true;
        }
        if (strcmp(name, "echo") == 0) {
            for (int index = 1; index < command.argc; ++index) {
                if (index > 1)
                    (void)putchar(' ');
                (void)write(STDOUT_FILENO, command.argv[index], strlen(command.argv[index]));
            }
            (void)putchar('\n');
            last_status = 0;
            return true;
        }
        return false; // not a builtin
    }

    void apply_redirections(const stage& command) noexcept {
        if (command.input_path != nullptr) {
            const int fd = open(command.input_path, O_RDONLY);
            if (fd >= 0)
                (void)dup2(fd, STDIN_FILENO);
        }
        if (command.output_path != nullptr) {
            const int flags =
                O_WRONLY | O_CREAT | (command.append ? O_APPEND : O_TRUNC);
            const int fd = open(command.output_path, flags);
            if (fd >= 0)
                (void)dup2(fd, STDOUT_FILENO);
        }
    }

    void run_external(const stage& command) noexcept {
        apply_redirections(command);
        (void)execv(command.argv[0], command.argv);
        // execv only returns on failure.
        (void)write(STDERR_FILENO, "sh: command not found\n", 23U);
        _exit(127);
    }

    // Called only for pipelines the caller has already determined are not
    // a single bare builtin invocation.
    void run_pipeline(stage (&stages)[max_stages], int stage_count) noexcept {
        char pipe_paths[max_stages][24];
        for (int index = 0; index < stage_count - 1; ++index)
            (void)snprintf(pipe_paths[index], sizeof(pipe_paths[index]), "/tmp/.pipe%d", index);

        for (int index = 0; index < stage_count; ++index) {
            stage& current = stages[index];
            if (index > 0)
                current.input_path = pipe_paths[index - 1];
            if (index < stage_count - 1)
                current.output_path = pipe_paths[index];

            if (current.argc == 0)
                continue;
            const int pid = fork();
            if (pid < 0) {
                last_status = 1;
                continue;
            }
            if (pid == 0)
                run_external(current);
            int status = 0;
            (void)waitpid(pid, &status, 0);
            last_status = status;
        }
    }
} // namespace

extern "C" int sys_user_main(int, char**, char**) noexcept {
    seed_vars_from_environ();
    for (;;) {
        (void)write(STDOUT_FILENO, "$ ", 2U);
        char line[line_capacity];
        const int length = read_line(line);
        if (length == 0)
            continue;

        token_list tokens{};
        if (!tokenize(line, tokens) || tokens.count == 0)
            continue;

        stage stages[max_stages];
        int stage_count = 0;
        if (!build_stages(tokens, stages, stage_count))
            continue;

        if (stage_count == 1 && stages[0].argc > 0 && is_builtin_name(stages[0].argv[0])) {
            /*
             * Builtins run in the shell's own process, not a fork, so a
             * redirection has to be applied to the shell's own stdin/
             * stdout and then undone afterward -- run_pipeline()'s
             * apply_redirections() only ever runs inside a forked child,
             * where there is nothing to restore. Saved onto fds 5/6,
             * clear of every fd a shell session normally has open.
             */
            const bool has_redirection =
                stages[0].input_path != nullptr || stages[0].output_path != nullptr;
            if (has_redirection) {
                (void)dup2(STDIN_FILENO, 5);
                (void)dup2(STDOUT_FILENO, 6);
                apply_redirections(stages[0]);
            }
            bool should_exit = false;
            (void)run_builtin(stages[0], should_exit);
            if (has_redirection) {
                (void)dup2(5, STDIN_FILENO);
                (void)dup2(6, STDOUT_FILENO);
                (void)close(5);
                (void)close(6);
            }
            if (should_exit)
                return last_status;
            continue;
        }
        run_pipeline(stages, stage_count);
    }
}
