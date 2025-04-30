module;

#include <sys/wait.h>
#include <unistd.h>

module HE;

bool HE::FileSystem::Open(const std::filesystem::path& path)
{
	const char* args[] { "xdg-open", path.c_str(), NULL };
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (!pid)
    {
        execvp(args[0], const_cast<char **>(args));
        exit(-1);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status) == 0;
    }
}
