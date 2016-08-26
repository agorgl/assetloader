#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

struct window
{
    GLFWwindow* wnd_handle;
    struct window_callbacks callbacks;
    void* userdata;
    /* Misc state */
    float cursor_pos[2], cursor_prev_pos[2];
};

static void glfw_err_cb(int code, const char* desc)
{
    fprintf(stderr, "Error %d, \t%s\n", code, desc);
    exit(1);
}

static void glfw_mouse_button_cb(GLFWwindow* wnd_handle, int button, int action, int mods)
{
    struct window* wnd = glfwGetWindowUserPointer(wnd_handle);
    if (wnd->callbacks.mouse_button_cb)
        wnd->callbacks.mouse_button_cb(wnd, button, action, mods);
}

static void glfw_cursor_pos_cb(GLFWwindow* wnd_handle, double xpos, double ypos)
{
    struct window* wnd = glfwGetWindowUserPointer(wnd_handle);
    if (wnd->callbacks.cursor_pos_cb)
        wnd->callbacks.cursor_pos_cb(wnd, xpos, ypos);
}

static void glfw_cursor_enter_cb(GLFWwindow* wnd_handle, int entered)
{
    struct window* wnd = glfwGetWindowUserPointer(wnd_handle);
    if (wnd->callbacks.cursor_enter_cb)
        wnd->callbacks.cursor_enter_cb(wnd, entered == GL_TRUE ? 1 : 0);
}

static void glfw_scroll_cb(GLFWwindow* wnd_handle, double xoff, double yoff)
{
    struct window* wnd = glfwGetWindowUserPointer(wnd_handle);
    if (wnd->callbacks.scroll_cb)
        wnd->callbacks.scroll_cb(wnd, xoff, yoff);
}

static void glfw_key_cb(GLFWwindow* wnd_handle, int key, int scancode, int action, int mods)
{
    struct window* wnd = glfwGetWindowUserPointer(wnd_handle);
    int act = -1;
    switch (action) {
        case GLFW_PRESS:
            act = KEY_ACTION_PRESS;
            break;
        case GLFW_RELEASE:
            act = KEY_ACTION_RELEASE;
            break;
        case GLFW_REPEAT:
            act = KEY_ACTION_REPEAT;
            break;
    }
    if (wnd->callbacks.key_cb)
        wnd->callbacks.key_cb(wnd, key, scancode, act, mods);
}

static void glfw_char_cb(GLFWwindow* wnd_handle, unsigned int codepoint)
{
    struct window* wnd = glfwGetWindowUserPointer(wnd_handle);
    if (wnd->callbacks.char_cb)
        wnd->callbacks.char_cb(wnd, codepoint);
}

static void glfw_char_mods_cb(GLFWwindow* wnd_handle, unsigned int codepoint, int mods)
{
    struct window* wnd = glfwGetWindowUserPointer(wnd_handle);
    if (wnd->callbacks.char_mods_cb)
        wnd->callbacks.char_mods_cb(wnd, codepoint, mods);
}

struct window* window_create(const char* title, int width, int height, int mode)
{
    /* Initialize glfw context */
    glfwInit();
    glfwSetErrorCallback(glfw_err_cb);

    struct window* wnd = malloc(sizeof(struct window));

    /* Open GL version hints */
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_FLOATING, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    /* Get monitor value according to given window mode */
    GLFWmonitor* mon = 0;
    switch (mode) {
        case 0: {
            mon = 0;
            break;
        }
        case 1: {
            mon = glfwGetPrimaryMonitor();

            const GLFWvidmode* videoMode = glfwGetVideoMode(mon);
            glfwWindowHint(GLFW_RED_BITS, videoMode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, videoMode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, videoMode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate);

            break;
        }
        case 2: {
            mon = glfwGetPrimaryMonitor();
            break;
        }
    }

    /* Create the window */
    GLFWwindow* window = glfwCreateWindow(width, height, title, mon, 0);
    if (!window)
        return 0;
    wnd->wnd_handle = window;

    /* Set it as the current opengl context */
    glfwMakeContextCurrent(wnd->wnd_handle);

    /* Disable vSync */
    glfwSwapInterval(0);

    /* Set window user pointer to this in order to be able to use our callbacks */
    glfwSetWindowUserPointer(wnd->wnd_handle, wnd);

    /* Initialize glfw callbacks to internal functions */
    glfwSetMouseButtonCallback(wnd->wnd_handle, glfw_mouse_button_cb);
    glfwSetCursorPosCallback(wnd->wnd_handle, glfw_cursor_pos_cb);
    glfwSetCursorEnterCallback(wnd->wnd_handle, glfw_cursor_enter_cb);
    glfwSetScrollCallback(wnd->wnd_handle, glfw_scroll_cb);
    glfwSetKeyCallback(wnd->wnd_handle, glfw_key_cb);
    glfwSetCharCallback(wnd->wnd_handle, glfw_char_cb);
    glfwSetCharModsCallback(wnd->wnd_handle, glfw_char_mods_cb);

    /* Load OpenGL extensions */
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    return wnd;
}

void window_destroy(struct window* wnd)
{
    glfwDestroyWindow(wnd->wnd_handle);
    free(wnd);

    /* Close glfw context */
    glfwTerminate();
}

void window_set_callbacks(struct window* wnd, struct window_callbacks* callbacks)
{
    wnd->callbacks = *callbacks;
}

void window_update(struct window* wnd)
{
    /* Poll events and fire callbacks */
    glfwPollEvents();
    /* Update cached state */
    wnd->cursor_prev_pos[0] = wnd->cursor_pos[0];
    wnd->cursor_prev_pos[1] = wnd->cursor_pos[1];
    double xpos, ypos;
    glfwGetCursorPos(wnd->wnd_handle, &xpos, &ypos);
    wnd->cursor_pos[0] = xpos;
    wnd->cursor_pos[1] = ypos;
}

void window_swap_buffers(struct window* wnd)
{
    glfwSwapBuffers(wnd->wnd_handle);
}

void window_set_userdata(struct window* wnd, void* userdata)
{
    wnd->userdata = userdata;
}

void* window_get_userdata(struct window* wnd)
{
    return wnd->userdata;
}

enum key_action window_key_state(struct window* wnd, enum key k)
{
    int glfw_state = glfwGetKey(wnd->wnd_handle, k);
    switch (glfw_state) {
        case GLFW_PRESS:
            return KEY_ACTION_PRESS;
        case GLFW_RELEASE:
            return KEY_ACTION_RELEASE;
        case GLFW_REPEAT:
            return KEY_ACTION_REPEAT;
        default:
            return KEY_ACTION_RELEASE;
    }
}

enum key_action window_mouse_button_state(struct window* wnd, enum mouse_button mb)
{
    int glfw_state = glfwGetMouseButton(wnd->wnd_handle, mb);
    switch (glfw_state) {
        case GLFW_PRESS:
            return KEY_ACTION_PRESS;
        case GLFW_RELEASE:
            return KEY_ACTION_RELEASE;
        default:
            return KEY_ACTION_RELEASE;
    }
}

void window_get_cursor_diff(struct window* wnd, float* x, float* y)
{
    if (x)
        *x = wnd->cursor_pos[0] - wnd->cursor_prev_pos[0];
    if (y)
        *y = wnd->cursor_pos[1] - wnd->cursor_prev_pos[1];
}

void window_grub_cursor(struct window* wnd, int mode)
{
    int glfw_mode = mode > 0 ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(wnd->wnd_handle, GLFW_CURSOR, glfw_mode);
}

int window_is_cursor_grubbed(struct window* wnd)
{
    return glfwGetInputMode(wnd->wnd_handle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}
