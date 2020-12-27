#include <iostream>
#include "gst/gst.h"

//needed for elements with "sometimes pads" e.g. flvdemux, tsdemux, decodebin
static void callbackNewPad(
    GstElement *element,
    GstPad *newPad,
    gpointer data)
{
    const gchar *padname = gst_pad_get_name(newPad);
    std::cout << "New pad " << padname << " created." << std::endl;

    GstPadLinkReturn ret;
    GstElement **queues = static_cast<GstElement **>(data);

    if ("video" == std::string(padname))
    {
        GstElement *video_queue = queues[0];
        GstPad *videoPad = gst_element_get_static_pad(video_queue, "sink");

        ret = gst_pad_link(newPad, videoPad);

        gst_object_unref(videoPad);
    }
    else if ("audio" == std::string(padname))
    {
        GstElement *audio_queue = queues[1];
        GstPad *audioPad = gst_element_get_static_pad(audio_queue, "sink");

        ret = gst_pad_link(newPad, audioPad);

        gst_object_unref(audioPad);
    }

    if (ret == GST_PAD_LINK_OK)
    {
        std::cout << "Pad " << padname << " dynamically linked." << std::endl;
    }
    else
    {
        std::cout << "Failed to link " << padname << "dynamically." << std::endl;
    }
}

int main(int argc, char *argv[])
{
    std::string srcRtmp;
    std::string dstRtmp;
    if(3 == argc)
    {
        srcRtmp = argv[1];
        dstRtmp = argv[2];
    }
    else
    {
        std::cout << "USAGE: " << argv[0] << " {source rtmp url} {destination rtmp url}" << std::endl;
        return -1;
    }

    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    gst_init(0, 0);

    GstElement *pipeline = gst_pipeline_new("pipeline");
    GstElement *rtmpsrc = gst_element_factory_make("rtmpsrc", nullptr);
    GstElement *flvdemux = gst_element_factory_make("flvdemux", nullptr);
    GstElement *video_queue = gst_element_factory_make("queue", nullptr);
    GstElement *audio_queue = gst_element_factory_make("queue", nullptr);
    GstElement *flvmux = gst_element_factory_make("flvmux", nullptr);
    GstElement *rtmpsink = gst_element_factory_make("rtmpsink", nullptr);   

    if(!pipeline || !rtmpsrc || !flvdemux || !video_queue || !audio_queue || !flvmux || !rtmpsink)
    {
        std::cout << "Failed to create elements." << std::endl;
        return -1;
    }
    
    g_object_set(rtmpsrc, "location", srcRtmp.c_str(), nullptr);
    g_object_set(flvmux, "streamable", true, nullptr);
    g_object_set(rtmpsink, "location", dstRtmp.c_str(), nullptr);

    gst_bin_add_many(GST_BIN(pipeline), rtmpsrc, flvdemux, audio_queue, video_queue, flvmux, rtmpsink, nullptr);
    gboolean linked = gst_element_link(rtmpsrc, flvdemux);
    if(!linked)
    {
        std::cout << "Failed to link rtmpsrc and flvdemux." << std::endl;
    }

    GstElement *queues[2] = {video_queue, audio_queue};
    g_signal_connect(flvdemux, "pad-added", G_CALLBACK(callbackNewPad), queues);

    GstPad *videoQueueSrcPad = gst_element_get_static_pad(video_queue, "src");
    GstPad *muxVideoPad = gst_element_get_request_pad(flvmux, "video");
    GstPadLinkReturn linkRet = gst_pad_link(videoQueueSrcPad, muxVideoPad);
    if(GST_PAD_LINK_OK != linkRet)
    {
        std::cout << "Failed to link flvmux video sink pad and queue src pad." << std::endl;
    }

    GstPad *audioQueueSrcPad = gst_element_get_static_pad(audio_queue, "src");
    GstPad *muxAudioPad = gst_element_get_request_pad(flvmux, "audio");
    linkRet = gst_pad_link(audioQueueSrcPad, muxAudioPad);
    if(GST_PAD_LINK_OK != linkRet)
    {
        std::cout << "Failed to link flvmux audio sink pad and queue src pad." << std::endl;
    }

    linked = gst_element_link(flvmux, rtmpsink);
    if(!linked)
    {
        std::cout << "Failed to link flvmux and rtmpsink." << std::endl;
    }

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        std::cout << "Unable to set the pipeline to playing state.";
        gst_object_unref(pipeline);
    }

    bus = gst_element_get_bus(pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                         (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

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
