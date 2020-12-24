#include <iostream>
#include "gst/gst.h"

//needed for elements with "sometimes pads" e.g. flvdemux, tsdemux, decodebin
static void callbackNewPad(
    GstElement *element,
    GstPad *newPad,
    gpointer data)
{
    const gchar *padname = gst_pad_get_name(newPad);
    std::cout << "New pad " << padname << " created!" << std::endl;

    GstElement **queues = static_cast<GstElement **>(data);
    GstElement *video_queue = queues[0];
    GstElement *audio_queue = queues[1];

    if ("video" == std::string(padname))
    {
        GstPad *videoPad = gst_element_get_static_pad(video_queue, "sink");

        GstPadLinkReturn ret = gst_pad_link(newPad, videoPad);
        if (ret == GST_PAD_LINK_OK)
        {
            std::cout << "Pad " << padname << " dynamically linked." << std::endl;
        }
    }
    else if ("audio" == std::string(padname))
    {
        GstPad *audioPad = gst_element_get_static_pad(audio_queue, "sink");

        GstPadLinkReturn ret = gst_pad_link(newPad, audioPad);
        if (ret == GST_PAD_LINK_OK)
        {
            std::cout << "Pad " << padname << " dynamically linked." << std::endl;
        }
    }
}

int main()
{
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    gst_init(0, 0);

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

    GstElement *pipeline = gst_pipeline_new("pipeline");

    GstElement *rtmpsrc = gst_element_factory_make("rtmpsrc", nullptr);
    //g_object_set(rtmpsrc, "location", "rtmp://34.105.124.140:1935/live/pri_test", nullptr);
    g_object_set(rtmpsrc, "location", "rtmp://s3b78u0kbtx79q.cloudfront.net/cfx/st/honda_accord", nullptr);

    GstElement *flvdemux = gst_element_factory_make("flvdemux", nullptr);
    GstElement *video_queue = gst_element_factory_make("queue", nullptr);
    GstElement *audio_queue = gst_element_factory_make("queue", nullptr);

    GstElement *flvmux = gst_element_factory_make("flvmux", nullptr);

    GstElement *rtmpsink = gst_element_factory_make("rtmpsink", nullptr);
    g_object_set(rtmpsink, "location", "rtmp://live-fra05.twitch.tv/app/live_161664427_KkrHvQK9VmfaGtZ9emFKFzOnkV6Cnc", nullptr);

    gst_bin_add_many(GST_BIN(pipeline), rtmpsrc, flvdemux, audio_queue, video_queue, flvmux, rtmpsink, nullptr);
    gboolean linked = gst_element_link(rtmpsrc, flvdemux);
    //gboolean linked = gst_element_link(filesrc, flvdemux);

    GstElement *queues[2] = {video_queue, audio_queue};
    g_signal_connect(flvdemux, "pad-added", G_CALLBACK(callbackNewPad), queues);

    
    GstPad *videoQueueSrcPad = gst_element_get_static_pad(video_queue, "src"); 
    GstPad *muxVideoPad = gst_element_get_request_pad(flvmux, "video");
    const gchar *padname = gst_pad_get_name(muxVideoPad);
    GstPadLinkReturn linkRet = gst_pad_link(videoQueueSrcPad, muxVideoPad);

    GstPad *audioQueueSrcPad = gst_element_get_static_pad(audio_queue, "src");
    GstPad *muxAudioPad = gst_element_get_request_pad(flvmux, "audio");
    const gchar *padname2 = gst_pad_get_name(muxAudioPad);
    linkRet = gst_pad_link(audioQueueSrcPad, muxAudioPad);

    linked = gst_element_link(flvmux, rtmpsink);

    // gboolean linked = gst_element_link_many(rtmpsrc, flvdemux, flvmux, rtmpsink, nullptr);

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        std::cout << "Unable to set the pipeline to playing state.";
        gst_object_unref(pipeline);
    }

    //g_main_loop_run (main_loop);

    bus = gst_element_get_bus(pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                         (GST_MESSAGE_STATE_CHANGED));

        /* Parse message */
        if (msg != NULL)
        {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg))
            {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                terminate = TRUE;
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                terminate = TRUE;
                break;
            case GST_MESSAGE_STATE_CHANGED:
                /* We are only interested in state-changed messages from the pipeline */
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline))
                {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    g_print("Pipeline state changed from %s to %s:\n",
                            gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
                }
                break;
            default:
                /* We should not reach here */
                g_printerr("Unexpected message received.\n");
                break;
            }
            gst_message_unref(msg);
        }
    } while (!terminate);

    return 0;
}
