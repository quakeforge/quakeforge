/***********************************************
*                                              *
*              FrikBot Misc Code               *
*   "Because you can't name it anything else"  *
*                                              *
***********************************************/

/*
This program is in the Public Domain. My crack legal
team would like to add:

RYAN "FRIKAC" SMITH IS PROVIDING THIS SOFTWARE "AS IS"
AND MAKES NO WARRANTY, EXPRESS OR IMPLIED, AS TO THE
ACCURACY, CAPABILITY, EFFICIENCY, MERCHANTABILITY, OR
FUNCTIONING OF THIS SOFTWARE AND/OR DOCUMENTATION. IN
NO EVENT WILL RYAN "FRIKAC" SMITH BE LIABLE FOR ANY
GENERAL, CONSEQUENTIAL, INDIRECT, INCIDENTAL,
EXEMPLARY, OR SPECIAL DAMAGES, EVEN IF RYAN "FRIKAC"
SMITH HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, IRRESPECTIVE OF THE CAUSE OF SUCH DAMAGES. 

You accept this software on the condition that you
indemnify and hold harmless Ryan "FrikaC" Smith from
any and all liability or damages to third parties,
including attorney fees, court costs, and other
related costs and expenses, arising out of your use
of this software irrespective of the cause of said
liability. 

The export from the United States or the subsequent
reexport of this software is subject to compliance
with United States export control and munitions
control restrictions. You agree that in the event you
seek to export this software, you assume full
responsibility for obtaining all necessary export
licenses and approvals and for assuring compliance
with applicable reexport restrictions. 

Any reproduction of this software must contain
this notice in its entirety. 
*/

#include "libfrikbot.h"

integer bot_chat_linker;

@static Bot b_originator;
@static integer b_topic;
/* FBX Topics

b_originator == self
 1 - sign on
 2 - killed targ
 3 - team message "friendly eyes"
 4 - team message "on your back"
 5 - team message "need back up"
 6 - excuses
 ----
 7 - gameover
 ----
 8 - welcoming someone onto server
 9 - ridicule lost frag (killed self?)
 10 - ridicule lost frag (lava)
 11 - lag
b_originator == targ
*/

@implementation Bot (Chat)

-(void)say:(string)msg
{
	WriteByte (MSG_ALL, 8);
	WriteByte (MSG_ALL, 3);
	WriteByte (MSG_ALL, 1);
	WriteString (MSG_ALL, ent.netname);
	WriteByte (MSG_ALL, 8);
	WriteByte (MSG_ALL, 3);
	WriteByte (MSG_ALL, 2);
	WriteString (MSG_ALL, msg);
}

-(void)say2:(string)msg
{
	WriteByte (MSG_ALL, 8);
	WriteByte (MSG_ALL, 3);
	WriteByte (MSG_ALL, 2);
	WriteString (MSG_ALL, msg);
}

-(void)sayTeam:(string)msg
{
	// FBX QW doesn't support teamplay...yet
}

-(void)sayInit
{
	WriteByte (MSG_ALL, 8);
	WriteByte (MSG_ALL, 3);
	WriteByte (MSG_ALL, 1);
	WriteString (MSG_ALL, ent.netname);
}

// I didn't like the old code so this is very stripped down
-(void)start_topic:(integer)topic
{
	if (random() < 0.2) {
		b_topic = topic;
		b_originator = self;
	} else
		b_topic = 0;
}

-(void)chat
{
	local float r;

	if (b_options & OPT_NOCHAT)
		return;

	r = ceil (6 * random ());

	if (b_chattime > time) {
		if (b_skill < 2)
			keys = 0;
			ent.button0 = ent.button2 = 0;
		return;
	} else if (b_chattime) {
		switch (b_topic) {
		case 1:
			if (b_originator == self) {
				switch (r) {
				case 1:
					[self say:": lo all\n"];
					[self start_topic:8];
					break;
				case 2:
					[self say:": hey everyone\n"];
					[self start_topic:8];
					break;
				case 3:
					[self say:": prepare to be fragged!\n"];
					[self start_topic:0];
					break;
				case 4:
					[self say:": boy this is laggy\n"];
					[self start_topic:11];
					break;
				case 5:
					[self say:": #mm getting some lag here\n"];
					[self start_topic:11];
					break;
				default:
					[self say:": hi everyone\n"];
					[self start_topic:8];
					break;
				}
			}
			break;
		case 2:
			if (b_originator == self) {
				switch (r) {
				case 1:
					[self say:": take that\n"];
					break;
				case 2:
					[self say:": yehaww!\n"];
					break;
				case 3:
					[self say:": wh00p\n"];
					break;
				case 4:
					[self say:": j00_sawk();\n"];
					break;
				case 5:
					[self say:": i rule\n"];
					break;
				default:
					[self say:": eat that\n"];
					break;
				}
				[self start_topic:0];
			}
			break;
		case 3:
			if (b_originator == self) {
				if (r < 3)
					[self sayTeam:": friendly eyes\n"];
				else
					[self sayTeam:": team eyes\n"];
				[self start_topic:0];
			}
			break;
		case 4:
			if (b_originator == self) {
				if (r < 3)
					[self sayTeam:": on your back\n"];
				else
					[self sayTeam:": I'm with you\n"];
				[self start_topic:0];
			}
			break;
		case 5:
			if (b_originator == self) {
				if (r < 3)
					[self sayTeam:": I need help\n"];
				else
					[self sayTeam:": need backup\n"];
				[self start_topic:0];
			}
			break;
		case 6:
			if (b_originator == self) {
				switch (r) {
				case 1:
					[self say:": sun got in my eyes\n"];
					[self start_topic:0];
					break;
				case 2:
					[self say:": mouse needs cleaning\n"];
					[self start_topic:0];
					break;
				case 3:
					[self say:": i meant to do that\n"];
					[self start_topic:0];
					break;
				case 4:
					[self say:": lag\n"];
					[self start_topic:11];
					break;
				case 5:
					[self say:": killer lag\n"];
					[self start_topic:11];
					break;
				default:
					[self say:": 100% lag\n"];
					[self start_topic:11];
					break;
				}
			}
			break;
		case 7:
			switch (r) {
			case 1:
				[self say:": gg\n"];
				break;
			case 2:
				[self say:": gg all\n"];
				break;
			case 3:
				[self say:": that was fun\n"];
				break;
			case 4:
				[self say:": good game\n"];
				break;
			case 5:
				[self say:": pah\n"];
				break;
			default:
				[self say:": hrm\n"];
				break;
			}
			[self start_topic:0];
			break;
		case 8:
			if (b_originator != self) {
				switch (r) {
				case 1:
					[self say:": heya\n"];
					[self start_topic:0];
					break;
				case 2:
					[self say:": welcome\n"];
					[self start_topic:0];
					break;
				case 3:
					[self sayInit];
					[self say2:": hi "];
					[self say2:b_originator.ent.netname];
					[self say2:"\n"];
					[self start_topic:0];
					break;
				case 5:
					[self sayInit];
					[self say2:": hey "];
					[self say2:b_originator.ent.netname];
					[self say2:"\n"];
					[self start_topic:0];
					break;
				case 6:
					[self say:": howdy\n"];
					[self start_topic:0];
					break;
				default:
					[self say:": lo\n"];
					[self start_topic:0];	
					break;
				}
			}
			break;
		case 9:
			if (b_originator != self) {
				switch (r) {
				case 1:
					[self say:": hah\n"];
					break;
				case 2:
					[self say:": heheh\n"];
					break;
				case 3:
					[self sayInit];
					[self say2:": good work "];
					[self say2:b_originator.ent.netname];
					[self say2:"\n"];
					break;
				case 4:
					[self sayInit];
					[self say2:": nice1 "];
					[self say2:b_originator.ent.netname];
					[self say2:"\n"];
					break;
				case 5:
					[self say:": lol\n"];
					break;
				default:
					[self say:": :)\n"];
					break;
				}
				b_topic = 6;
			}
			break;
		case 10:
			if (b_originator != self) {
				switch (r) {
				case 1:
					[self say:": have a nice dip?\n"];
					break;
				case 2:
					[self say:": bah I hate levels with lava\n"];
					break;
				case 3:
					[self sayInit];
					[self say2:": good job "];
					[self say2:b_originator.ent.netname];
					[self say2:"\n"];
					break;
				case 4:
					[self sayInit];
					[self say2:": nice backflip "];
					[self say2:b_originator.ent.netname];
					[self say2:"\n"];
					break;
				case 5:
					[self say:": watch your step\n"];
					break;
				default:
					[self say:": hehe\n"];
					break;
				}
				b_topic = 6;
			}
			break;
		case 11:
			if (b_originator != self) {
				switch (r) {
				case 1:
					[self sayInit];
					[self say2:": yeah right "];
					[self say2:b_originator.ent.netname];
					[self say2:"\n"];
					[self start_topic:0];
					break;
				case 2:
					[self say:": ping\n"];
					[self start_topic:0];
					break;
				case 3:
					[self say:": shuddup, you're an lpb\n"];
					[self start_topic:0];
					break;
				case 4:
					[self say:": lag my eye\n"];
					[self start_topic:0];
					break;
				case 5:
					[self say:": yeah\n"];
					[self start_topic:11];
					break;
				default:
					[self say:": totally\n"];
					[self start_topic:11];
					break;
				}
			}
		default:
			break;
		}
		b_chattime = 0;
	} else if (b_topic) {
		if (random () < 0.5) {
			if (self == b_originator) {
				if (b_topic <= 7)
					b_chattime = time + 2;
			} else {
				if (b_topic >= 7)
					b_chattime = time + 2;
			}
		}
	}
}
@end
