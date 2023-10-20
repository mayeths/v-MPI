#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <vector>
#include <string>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "framework/GUI.hpp"
#include "framework/Shader.hpp"
#include "framework/Window.hpp"
#include "util/log.h"
#include "SmileBox.hpp"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/event.h>

#define MYS_IMPL
#define MYS_NO_MPI
#include <mys.h>

#define LOCAL_PORT  9914
#define REMOTE_PORT 8081
#define REMOTE_HOST "166.111.69.35"
#define REMOTE_USER "huanghp"

#include "helper.hpp"

pid_t helper_pid;

// TODO：有四类signals, Fatal Signals/Termination Signals/User-Initiated Signals/Ignored Signals/
// 对于fatal signal和Termination Signals，应该清理资源然后退出（主要是close接口，因为unix类的哲学是资源是文件）。
// 对于user initiated signals，应该graceful exit，比如send/write一些byebye给用户
// 对于ignored signals，应该在下一次启动的时候检查上次资源是否被释放。所以，要设计一个框架能够检查系统内的别的运行进程的生存情况。

// 目前，我们还是所有实例都尝试绑定某一个固定端口。这样，如果我们代码没处理好，我们就能及时崩溃报错。


#define MAX_CONN        16
#define MAX_EVENTS      32
#define BUF_SIZE        1024

int sock_fd;
char buf[BUF_SIZE];
struct sockaddr_in srv_addr;
int kq;
struct kevent events[MAX_EVENTS];
struct timespec timeout;
/*
 * test clinet 
 */
int client_connect(const char *server_host, int server_port)
{
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&srv_addr, 0, sizeof(struct sockaddr_in));
	srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(server_host);
	srv_addr.sin_port = htons(server_port);

    printf("MASTER-%d: Connecting to %s:%d\n", getpid(), server_host, server_port);
    kq = kqueue();
    struct kevent kev_read;

    EV_SET(&kev_read, sock_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(kq, &kev_read, 1, NULL, 0, NULL) == -1) {
        perror("kevent (add read)");
        exit(1);
    }
    if (connect(sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) != 0) {
        if (errno != EINPROGRESS) {
            perror("Connect");
            exit(errno);
        }
    }

    /*---Wait for socket connect to complete---*/
    timeout.tv_sec = 5;
    timeout.tv_nsec = 0;
    int num_ready = kevent(kq, NULL, 0, events, MAX_EVENTS, &timeout);
    for (int i = 0; i < num_ready; i++) {
        if (events[i].filter == EVFILT_READ && (events[i].flags & EV_EOF || events[i].flags & EV_ERROR)) {
            perror("kevent");
            exit(1);
        }
    }

    return sock_fd;
}

int factor_two(int x)
{
    for (int i = int(sqrt(x)); i >= 1; i--) {
        if (x % i == 0)
            return i;
    }
    return x;
}

int main() {
    helper_pid = establish_ssh_forward_tunnel(LOCAL_PORT, REMOTE_PORT, REMOTE_HOST, REMOTE_USER);
    usleep(5 * 1000 * 1000);
    client_connect("127.0.0.1", LOCAL_PORT);

    GLint success = 0;
    Window window(&success, "vMPI", 1920, 1080);
    if (success != 1) {
        log_fatal("Failed to create GLFW window");
        return -1;
    }
    window.EnableStatisticGUI(Window::STAT_FPS | Window::STAT_MEMORY | Window::STAT_POSITION);

    Camera &camera = window.camera;
    {
        camera.SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));
        camera.SetMovementSpeed(15);
        camera.SetMaxRenderDistance(1e8f);
    }

    int nranks = 1024;
    int ny = factor_two(nranks);
    int nx = nranks / ny;
    AS_EQ_I32(nx * ny, nranks);

    std::vector<SmileBox> smileBoxs(nranks);
    for (int i = 0; i < smileBoxs.size(); i++) {
        smileBoxs[i].SetShaderPath("assets/shaders/smilebox.vs", "assets/shaders/smilebox.fs");
        smileBoxs[i].SetTexturePath("assets/textures/container.jpg", "assets/textures/awesomeface.png");
        smileBoxs[i].Setup(i);
        int x = i % nx;
        int y = i / nx;
        int px = (-(float)nx / 2 + x) * 2;
        int py = y;
        int pz = -40.0f;
        glm::vec3 position = glm::vec3(px, py, pz);
        smileBoxs[i].MoveTo(position);
        smileBoxs[i].SetVisibility(false);
        window.AddObject(&smileBoxs[i]);
    }

	int socket_closed = 0;
    double now;
    double lastUpdateTime = 0;
    double lastRenderTime = 0;
    double FPSlimits = 60; // std::numeric_limits<double>::infinity()
    while (window.ContinueLoop()) {
        now = glfwGetTime();
        ////// Update Logic
        window.Update(now, lastUpdateTime);
        lastUpdateTime = now;
        if (!socket_closed) {
            struct timespec timeout;
            timeout.tv_sec = 0;
            timeout.tv_nsec = 0;
            int num_ready = kevent(kq, NULL, 0, events, MAX_EVENTS, &timeout);
            if (num_ready == -1) {
                perror("kevent (wait)");
                exit(1);
            }
            for (int i = 0; i < num_ready; i++) {
                AS_EQ_I16(events[i].filter, EVFILT_READ);
                if (events[i].flags & EV_EOF || events[i].flags & EV_ERROR) {
                    socket_closed = 1;
                    break;
                }
                bzero(buf, sizeof(buf));
                int readlen = (int)recv(sock_fd, buf, sizeof(buf), 0);
                if (readlen != sizeof(buf)) {
                    printf("Client: Get wrong length, expect %ld but %d\n", sizeof(buf), readlen);
                    exit(1);
                }
                printf("Client: get message |%s|\n", buf);
                int rank = -1;
                if (strstr(buf, "start") != NULL) {
                    sscanf(buf, "start %d", &rank);
                    smileBoxs[rank].SetVisibility(true);
                } else if (strstr(buf, "down") != NULL) {
                    sscanf(buf, "down %d", &rank);
                    AS_BETWEEN_IE_I32(0, rank, nranks);
                    smileBoxs[rank].SetCrashed();
                }
                if (rank != -1) {

                }
            }
        }
        ////// Render Frame
        if (now - lastRenderTime >= 1 / FPSlimits) {
            window.Render(now, lastRenderTime);
            lastRenderTime = now;
        }
    }

	close(sock_fd);
    close(kq);
    return 0;
}