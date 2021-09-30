/* FILE: oxswarn.h                 -*-Mode: c++-*-
 *
 * Macros and classes for nonfatal message handling.
 *
 */

#ifndef _OXS_WARN
#define _OXS_WARN

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "oc.h"
#include "nb.h"
#include "oxsthread.h"

OC_USE_STRING;

/* End includes */

/**********************************************************************
 *
 * NOTE: Any file wishing to use the Oxs_WarningMessage class should
 *  create a static const instance of Oxs_WarningMessageFileRevisionInfo,
 *  using the Date, Author, and Revision info from CVS using "keyword
 *  substitution," plus the C __FILE__ macro, like so:
 *
 *  static const Oxs_WarningMessageRevisionInfo revinfo
 *    (__FILE__,
 *     "$Revision: 1.9 $",
 *     "$Date: 2008/09/09 23:58:01 $",
 *     "$Author: donahue $",
 *     "Billy Bob");
 *
 *  The warning messages can then be sent by instantiating a static
 *  instance of the Oxs_WarningMessage, and calling its Send() method:
 *
 *     static Oxs_WarningMessage bar(3);
 *     bar.Send(revinfo,OC_STRINGIFY(__LINE__),
 *              "The fox is in the henhouse.");
 *
 *  The parameter "3" in the constructor specifies the maximum number
 *  of times this message will be sent.  Use -1 to specify no limit.
 *  Variable bar must be static to store the message id associated
 *  with the message.  The actual message count are stored in the
 *  Oxs_WarningMessage::message_counts class static variable.
 *
 **********************************************************************/

struct Oxs_WarningMessageRevisionInfo {
public:
  Oxs_WarningMessageRevisionInfo(const char* file_,
                                 const char* revision_,
                                 const char* date_,
                                 const char* revision_author_,
                                 const char* primary_author_);
  String file;
  String revision;
  String date;
  String revision_author;
  String primary_author;
private:
  static String StripRevmarks(const char* revinfo);
};

class Oxs_Director; // Forward reference

class Oxs_WarningMessage {
public:
  Oxs_WarningMessage(int count_max,Nb_MessageType mt = NB_MSG_WARNING)
    : max_message_count(count_max),
      message_type(mt) {
    Oc_LockGuard lck(mutex);
    instance_id = ++ids_in_use;
  }

  // If called from the root thread, Send() transmits the message
  // immediately.  When called from a child thread the message is
  // instead stored in the message hold which should be transmitted
  // when the thread joins the root thread.  Warning messages
  // generated in threads which never join (e.g. Oxs_ThreadThrowAway)
  // are not well handled at present---but they should be reported
  // upon problem termination (in Oxs_Director::Release()).
  // Unfortunately, in the last case if problem release is not clean
  // then potentially helpful warning messages may be lost.
  int Send(const Oxs_WarningMessageRevisionInfo& revinfo,
           const char* line,const char* msg,
           const char* src=NULL,const char* id=NULL);

  // The design intention is for clients to call Send; HoldMessage and
  // TransmitMessageHold are intended as helper methods for Send in
  // the case Send is called from a non-root thread.  However,
  // TransmitMessageHold needs to be called by Oxs_ThreadTree,
  // Oxs_ThreadBush, and Oxs_Director to make automatic dumps of the
  // hold, so both HoldMessage and TransmitMessageHold are made
  // public.
  int HoldMessage(const Oxs_WarningMessageRevisionInfo& revinfo,
                  const char* line,const char* msg,
                  const char* src,const char* id=NULL);
  // Call from any thread.  Message is stored and not transmitted
  // until the main thread call TransmitMessageHold.

  static int TransmitMessageHold();
  /// Call only from main thread. Return is number of messages transmitted.


  int GetMaxCount() const {
    return max_message_count;
  }
  int GetCurrentCount() const;
  void SetCurrentCount(int newcount);

private:
  int instance_id;  // Initialized in constructor; treat as constant
  const int max_message_count; // -1 => no limit
  const Nb_MessageType message_type;
  static Oc_Mutex mutex;
  static int ids_in_use;
  static map<int,int> message_counts;    // Maps id -> message count

  struct MessageData {
    Nb_MessageType msgtype;
    String text;
    String errCode;
    MessageData() : msgtype(NB_MSG_WARNING) {}
    MessageData(const Nb_MessageType& in_type,
                const String& in_text,
                const String& in_errCode)
      : msgtype(in_type), text(in_text), errCode(in_errCode) {}
  };
  static deque<MessageData> message_hold;
  // NOTE 1: Store message counts and hold centrally to provide easy
  //         support for the ClearHoldAndCounts() facility.
  // NOTE 2: In the future, we might want to add support for ganging
  //         together multiple warning messages as a single type for
  //         message output counts purposes.  This can be done in the
  //         present design by using the same instance for output
  //         at multiple places; this is probably not inconvenient
  //         inside a single file, but is more cumbersome if one wants
  //         to support this behavior across multiple files.

  static void ClearHoldAndCounts() {
    // Drops any messages stored in the hold and clears all message counts.
    Oc_LockGuard lck(mutex);
    message_counts.clear();
    message_hold.clear();
  }

  int FormatMessage
  (const Oxs_WarningMessageRevisionInfo& revinfo,
   const char* line,
   const char* msg,
   const char* src,
   const char* id,
   String& formatted_message, // Export
   String& errCode);          // Export

  friend class Oxs_Director;  // So Oxs_Director can call ClearCounts().
  Oxs_WarningMessage& operator=(const Oxs_WarningMessage&); // Declare
  /// but don't define, just to quiet some noisy compilers.  The
  /// assignment operator can't be defined because of the constant data
  /// members.
};

#endif // _OXS_WARN
