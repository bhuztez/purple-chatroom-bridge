#define PURPLE_PLUGINS
#include <glib.h>

#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#define PLUGIN_NAME "Purple Chatroom Bridge"
#define PLUGIN_ID "purple-chatroom-bridge"
#define PLUGIN_AUTHOR "bhuztez<bhuztez@gmail.com>"
#define VERSION "0.01"
#define PLUGIN_SUMMARY "Bridge Chatrooms"
#define PLUGIN_DESCRIPTION                        \
  "Bridge Chatrooms"
#define PLUGIN_HOMEPAGE "https://github.com/bhuztez/purple-chatroom-bridge"

#include <time.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <plugin.h>
#include <pluginpref.h>
#include <debug.h>
#include <xmlnode.h>

#include <account.h>
#include <blist.h>
#include <conversation.h>
#include <version.h>

static GList *chatroom_list = NULL;

static char*
xmlnode_get_content(xmlnode *node)
{
  GString *text = g_string_new("");
  xmlnode *c;
  char *esc;
  
  for(c = node->child; c; c = c->next) {
    if(c->type == XMLNODE_TYPE_TAG) {
      int esc_len;
      esc = xmlnode_to_str(c, &esc_len);
      text = g_string_append_len(text, esc, esc_len);
      g_free(esc);
    } else if(c->type == XMLNODE_TYPE_DATA && c->data_sz > 0) {
      esc = g_markup_escape_text(c->data, c->data_sz);
      text = g_string_append(text, esc);
      g_free(esc);
    }
  }
  
  return g_string_free(text, FALSE);
}

static char*
format_message(char *sender,
               char *message)
{
    GString* format_message = g_string_new("");
    xmlnode* message_node = xmlnode_from_str(message, -1);

    /* raw */
    if ( !message_node || 
         !( strcmp(message_node->name, "html")==0 ||
            strcmp(message_node->name, "body")==0 )) {
      g_string_printf(format_message, "%s: %s", sender, message);
      return g_string_free(format_message, FALSE);
    }

    xmlnode* body_node =
      (strcmp(message_node->name, "body")) ?
      xmlnode_get_child(message_node, "body") :
      message_node;

    char* message_content = xmlnode_get_content(body_node);
    g_string_printf(format_message, "%s: %s", sender, message_content);
    g_free(message_content);

    xmlnode_free(message_node);

    return g_string_free(format_message, FALSE);
}


static gboolean
received_chat_msg_cb(PurpleAccount *account,
                 char *sender,
                 char *message,
                 PurpleConversation *conv,
                 PurpleMessageFlags flags,
                 gpointer data)
{
  PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);

  purple_debug_info(PLUGIN_ID, "%s\n", message);

  /* only listen to rooms on the list */
  if (!g_list_find(chatroom_list, chat)) return FALSE;

  /* stop duplicated messages from QQ Qun */
  if (!strcmp(account->protocol_id, "prpl-qq") &&
      (account->alias) &&
      !strcmp(sender, account->alias)) return FALSE;

  /* if received, push to other chatrooms on the list */
  if ( (flags & PURPLE_MESSAGE_RECV) ) {
    char *msg = format_message(sender, message);

    void forward_chat_message(gpointer data, gpointer user_data) {
      PurpleConvChat *chatroom = (PurpleConvChat*)data;

      if(chatroom != chat) {
        purple_conv_chat_send(chatroom, msg);
      }
    }

    g_list_foreach(chatroom_list, forward_chat_message, NULL);

    g_free(msg);
  }

  return FALSE;
}

static void
deleting_conversation_cb(PurpleConversation *conv,
                         gpointer data)
{
  /* if it is a chatroom */
  if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
    PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);

    if (g_list_find(chatroom_list, chat)) {
      chatroom_list = g_list_remove(chatroom_list, chat);
    }
  }
}

static void
bridge_chatroom_cb(PurpleConversation *conv,
                   gpointer data)
{
  PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
  if (g_list_find(chatroom_list, chat)) {
    /* already bridged */
    chatroom_list = g_list_remove(chatroom_list, chat);

    purple_conv_chat_write(chat, PLUGIN_ID, "chatroom unbridged!", PURPLE_MESSAGE_SYSTEM, time(NULL));
  } else {
    /* to bridge */
    chatroom_list = g_list_append(chatroom_list, chat);

    purple_conv_chat_write(chat, PLUGIN_ID, "chatroom bridged!", PURPLE_MESSAGE_SYSTEM, time(NULL));
  }
}

static void
conversation_extended_menu_cb(PurpleConversation *conv,
                              GList **list,
                              gpointer data)
{
  PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
  PurpleMenuAction *action =
    purple_menu_action_new("toggle bridge",
                           PURPLE_CALLBACK(bridge_chatroom_cb),
                           NULL,
                           NULL);
  *list = g_list_append(*list, action);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
  purple_signal_connect(purple_conversations_get_handle(),
                        "received-chat-msg",
                        plugin,
                        PURPLE_CALLBACK(received_chat_msg_cb),
                        NULL);

  purple_signal_connect(purple_conversations_get_handle(),
                        "deleting-conversation",
                        plugin,
                        PURPLE_CALLBACK(deleting_conversation_cb),
                        NULL);

  purple_signal_connect(purple_conversations_get_handle(),
                        "conversation-extended-menu",
                        plugin,
                        PURPLE_CALLBACK(conversation_extended_menu_cb),
                        NULL);

  return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
  purple_signals_disconnect_by_handle(plugin);

  if (chatroom_list) {
    g_list_free(chatroom_list);
    chatroom_list = NULL;
  }

  return TRUE;
}

static PurplePluginInfo info = {
  PURPLE_PLUGIN_MAGIC,        /* magic number */
  PURPLE_MAJOR_VERSION,       /* purple major */
  PURPLE_MINOR_VERSION,       /* purple minor */
  PURPLE_PLUGIN_STANDARD,     /* plugin type */
  NULL,                       /* UI requirement */
  0,                          /* flags */
  NULL,                       /* dependencies */
  PURPLE_PRIORITY_DEFAULT,    /* priority */

  PLUGIN_ID,                  /* id */
  PLUGIN_NAME,                /* name */
  VERSION,                    /* version */
  PLUGIN_SUMMARY,             /* summary */
  PLUGIN_DESCRIPTION,         /* description */
  PLUGIN_AUTHOR,              /* author */
  PLUGIN_HOMEPAGE,            /* homepage */

  plugin_load,                /* load */
  plugin_unload,              /* unload */
  NULL,                       /* destroy */

  NULL,                       /* ui info */
  NULL,                       /* extra info */
  NULL,                       /* prefs info */
  NULL,                       /* actions */
  NULL,                       /* reserved */
  NULL,                       /* reserved */
  NULL,                       /* reserved */
  NULL                        /* reserved */
};

static void
init_plugin(PurplePlugin *plugin)
{

}

PURPLE_INIT_PLUGIN(purple_bridge, init_plugin, info)

