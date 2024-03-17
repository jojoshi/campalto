//
// Created by joschi on 3/16/24.
//
#define _POSIX_C_SOURCE 200112L

#include <wayland-server-core.h>
#include <assert.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <stdio.h>
#include <wlr/types/wlr_output.h>
#include <wayland-util.h>
#include <stdlib.h>
#include <time.h>

struct cpo_server {
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;

    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;

    struct wlr_backend *backend;

    //LISTENERS
    struct wl_listener new_output;

    struct wl_list outputs;
};

struct cpo_output {

    struct wlr_output *wlr_output;
    struct cpo_server *server;
    struct timespec last_frame;

    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;

    struct wl_list link;
};

static void output_frame_notify(struct wl_listener *listener, void *data) {

    struct cpo_output *output = wl_container_of(listener, output, frame);
    struct wlr_output *wlr_output = data;
    struct wlr_renderer *renderer = output->server->renderer;
    assert(renderer);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int width, height;
    wlr_output_effective_resolution(wlr_output, &width, &height);

    wlr_output_attach_render(wlr_output, NULL);
    wlr_renderer_begin(renderer, width, height);

    float color[4] = {1.0, 0, 0, 1.0};
    wlr_renderer_clear(renderer, color);

    wlr_renderer_end(renderer);
    wlr_output_commit(wlr_output);

    output->last_frame = now;

}

static void output_destroy_notify(struct wl_listener *listener, void *data){

    struct cpo_output *output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);

}

static void output_request_state(struct wl_listener *listener, void *data) {
    /* This function is called when the backend requests a new state for
     * the output. For example, Wayland and X11 backends request a new mode
     * when the output window is resized. */
    struct cpo_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
}

static void new_output_notify(struct wl_listener *listener, void *data){

    struct cpo_server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode); //apply mode to state
    }
    wlr_output_commit_state(wlr_output, &state); //apply changes to state
    wlr_output_state_finish(&state);

    struct cpo_output *output = calloc(1, sizeof(*output));
    output->wlr_output = wlr_output;
    output->server = server;
    wl_list_insert(&server->outputs, &output->link);

    output->frame.notify = output_frame_notify;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    output->destroy.notify = output_destroy_notify;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

}

int main(int argc, char *argv[]) {

    struct cpo_server server;

    server.wl_display = wl_display_create();
    assert(server.wl_display);
    server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
    assert(server.wl_event_loop);

    server.backend = wlr_backend_autocreate(server.wl_display, NULL);
    assert(server.backend);

    server.renderer = wlr_renderer_autocreate(server.backend);
    if (server.renderer == NULL) {
        fprintf(stderr, "Failed to create renderer\n");
        return 1;
    }

    wlr_renderer_init_wl_display(server.renderer, server.wl_display);

    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    if (server.allocator == NULL) {
        fprintf(stderr, "failed to create wlr_allocator");
        return 1;
    }

    //Handle outputs and creation of new outputs
    wl_list_init(&server.outputs);
    server.new_output.notify = new_output_notify;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) {
        wlr_backend_destroy(server.backend);
        return 1;
    }

    if(!wlr_backend_start(server.backend)){
        fprintf(stderr, "Failed to start backend\n");
        wl_display_destroy(server.wl_display);
        wlr_backend_destroy(server.backend);
        return -1;
    }

    wl_display_run(server.wl_display);
    wl_display_destroy(server.wl_display);
    return 0;
}