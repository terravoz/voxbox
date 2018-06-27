<?php

/**
 * @file
 *   Messaging VOIP scripts, and related script theme functions.
 */

/**
 * Script callbacks.
 */

/**
 * Script to send a single message.
 */
function messaging_voip_script_message_send($message) {
  $script = new VoipScript('messaging_voip_script_message_send');
  $script->setVar('introduction', theme('messaging_voip_message_send_intro'));
  $script->setVar('message', $message);
  $script->setVar('goodbye', theme('messaging_voip_message_send_end'));

  $script->addSay('%introduction');
  $script->addLabel('play message');
  $script->addSay('%message');
  $script->addLabel('get input');
  $text = t('To play this message again press 1. To hangup, press the # key.');
  $timeout = 5;
  $end_key = '';
  $num_digits = 1;
  $script->addGetInput($text, $num_digits, $end_key, $timeout);
  $script->addGotoIf('play message', "^%input_digits == '1'");
  $script->addGotoIf('end call', "^%input_digits' == '#'");
  // Wrong input.
  $script->addSay(t('Invalid input received. Please try again.'));
  $script->addGoto('get input');
  // Terminate Call.
  $script->addLabel('end call');
  $script->addSay('%goodbye');
  $script->addHangup();

  return $script;
}

/**
 * Script to call out with messages in the index.
 */
function messaging_voip_script_croncall($account) {
  if (! is_object($account)) {
    $account = user_load($account);
  }

  $script = new VoipScript('messaging_voip_script_croncall');
  $script->setVar('introduction', theme('messaging_voip_message_send_intro', $account));
  $script->setVar('goodbye', theme('messaging_voip_message_send_end'));

  $script->addSay('%introduction');
  $script->addGosub('messaging_voip_script_store_index', array('account' => $account->uid));
  $script->addSay('%goodbye');
  $script->addHangUp();

  return $script;
}

/**
 * Set '%messaging_voip_store_count' with number of messages for user.
 *
 * @param int $uid
 *   (optional) User ID. Otherwise logged in user used. VoipScript
 *   $param['uid'].
 */
function messaging_voip_script_store_count($uid = NULL) {
  if ($uid == NULL) {
    global $user;
    $account = $user;
  }
  else {
    $account = user_load($uid);
  }

  $script = new VoipScript('messaging_voip_script_store_count');
  if (! $account->uid) {
    $script->addSet('messaging_voip_store_count', 0);
    $script->addReturn();
  }
  else {
    $messages = messaging_voip_store_get($account);
    $count  = count($messages);
    $script->addSet('messaging_voip_store_count', $count);
    $script->addReturn();
  }

  return $script;
}


/**
 * Index message, option to read or return, must be called with messages.
 *
 * Log the user in before calling this script.
 */
function messaging_voip_script_store_index() {
  global $user;
  $account = $user;

  $script = new VoipScript('messaging_voip_script_store_index');
  $script->addGosub('messaging_voip_script_store_count');

  $script->addGotoIf('read_messages', '^%messaging_voip_store_count > 0');

  $script->addSay('There are no messages.');
  $script->addReturn();

  $script->addLabel('read_messages');
  $script->addSay('You have %messaging_voip_store_count messages. First message: ');
  // @todo Offer option:
  //   1. hear messages.
  //   2. delete all messages.
  //   3. return.
  $script->addGosub(
    'voipviews_read_view',
    array(
      'view_name' => 'messaging_inbox',
      'display_id' => 'voipviews_1',
      'args' => json_encode(array($account->uid)),
    )
  );
  $script->addReturn();

  return $script;
}

/**
 * Script callback: Delete a message.
 *
 * Example:
 *   $script->addGosub(
 *     'messaging_voip_script_message_delete',
 *     array(
 *       'args' => json_encode(array('mqid' => $mqid )),
 *     )
 *   );
 *
 * @param array $args
 *   Arguments for messaging_store('del', $args).
 * @param string $say
 *   (optional) Message to be said on success.
 *   Default: 'Message deleted'.
 */
function messaging_voip_script_message_delete($args, $say = 'Message deleted') {
  $script = new VoipScript('messaging_voip_script_message_delete');
  messaging_store('del', $args);
  $script->addSay($say);
  $script->addReturn();
  return $script;
}


/**
 * Incoming calls script: collecting notifications.
 *
 * Logs user in and runs inbox script.
 */
function messaging_voip_script_store_incoming($params = NULL) {
  $script = new VoipScript('messaging_voip_script_store_incoming');
  $script->addGosub('voipusernumber_autologin', array('number' => '%caller_number'));
  // @Todo can handle user not existing user.
  //   Default behaviour, just say don't know you.
  //   Each site will want to override and say how you can subscribe, or
  //   not offer this until user is logged in.
  $script->addGosub('messaging_voip_script_store_index');

  return $script;
}

/****
 * Theme functions.
 *
 * @todo t('', array(), $language); // add user langage.
 * @todo format_plural($count, '', '', array(), $language); // add user langage.
 */

/**
 * Theme introduction to an outgoing messaging call.
 *
 * @ingroup themeable.
 *
 * @return string
 *   Text to be said.
 */
function theme_messaging_voip_message_send_intro($account = NULL) {
  $name = is_object($account) ? $account->name : '';
  $text = t('Hello @name, this is a message from the website @sitename', array('@name' => $name, '@sitename' => variable_get('site_name', 'Drupal')));
  $text .= t('You have a message.');

  return $text;
}

/**
 * Theme end to an outgoing messaging call.
 *
 * @ingroup themeable.
 *
 * @return string
 *   Text to be said.
 */
function theme_messaging_voip_message_send_end() {
  $text = t('Goodbye');

  return $text;
}

/**
 * Theme the messaging store index.
 *
 * Read out if someone calls up for their messages (and there are some),
 * or if they have been called with messages in the queue.
 *
 * @todo theme the options?
 *
 * @ingroup themeable.
 *
 * @param int $count
 *   Number of messages.
 * @param array $messages
 *   Array of messages, useful for example if you want to read out titles.
 *
 * @return string
 *   Text to be read out.
 */
function theme_messaging_voip_store_index($count, $messages) {
  $text = format_plural($count, 'There is one message', 'There are @count messages');
  return $text;
}

/**
 * Theme no messages pending.
 *
 * Read out if someone has called up and there are no messages in the queue.
 *
 * @ingroup themeable.
 *
 * @return string
 *   Text to be read out.
 */
function theme_messaging_voip_store_nomessages() {
  // @todo probably to trivial, can be in _script_
  $text = t('There are no notifications waiting');

  return $text;
}
