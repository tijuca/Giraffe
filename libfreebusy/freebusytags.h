/*
 * Copyright 2005 - 2012  Zarafa B.V.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3, 
 * as published by the Free Software Foundation with the following additional 
 * term according to sec. 7:
 *  
 * According to sec. 7 of the GNU Affero General Public License, version
 * 3, the terms of the AGPL are supplemented with the following terms:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V. The licensing of
 * the Program under the AGPL does not imply a trademark license.
 * Therefore any rights, title and interest in our trademarks remain
 * entirely with us.
 * 
 * However, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the
 * Program. Furthermore you may use our trademarks where it is necessary
 * to indicate the intended purpose of a product or service provided you
 * use it in accordance with honest practices in industrial or commercial
 * matters.  If you want to propagate modified versions of the Program
 * under the name "Zarafa" or "Zarafa Server", you may only do so if you
 * have a written permission by Zarafa B.V. (to acquire a permission
 * please contact Zarafa at trademark@zarafa.com).
 * 
 * The interactive user interface of the software displays an attribution
 * notice containing the term "Zarafa" and/or the logo of Zarafa.
 * Interactive user interfaces of unmodified and modified versions must
 * display Appropriate Legal Notices according to sec. 5 of the GNU
 * Affero General Public License, version 3, when you propagate
 * unmodified or modified versions of the Program. In accordance with
 * sec. 7 b) of the GNU Affero General Public License, version 3, these
 * Appropriate Legal Notices must retain the logo of Zarafa or display
 * the words "Initial Development by Zarafa" if the display of the logo
 * is not reasonably feasible for technical reasons. The use of the logo
 * of Zarafa in Legal Notices is allowed for unmodified and modified
 * versions of the software.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */


#ifndef FREEBUSYTAG_INCLUDED
#define FREEBUSYTAG_INCLUDED

#define PR_FREEBUSY_ALL_EVENTS			PROP_TAG(PT_MV_BINARY, 0x6850)
#define PR_FREEBUSY_ALL_MONTHS			PROP_TAG(PT_MV_LONG, 0x684F)
#define PR_FREEBUSY_BUSY_EVENTS			PROP_TAG(PT_MV_BINARY, 0x6854)
#define PR_FREEBUSY_BUSY_MONTHS			PROP_TAG(PT_MV_LONG, 0x6853)

#define PR_FREEBUSY_EMAIL_ADDRESS		PROP_TAG(PT_TSTRING, 0x6849)  // PR_FREEBUSY_EMA
#define PR_FREEBUSY_END_RANGE			PROP_TAG(PT_LONG, 0x6848)
#define PR_FREEBUSY_LAST_MODIFIED		PROP_TAG(PT_SYSTIME, 0x6868)
#define PR_FREEBUSY_NUM_MONTHS			PROP_TAG(PT_LONG, 0x6869)  
#define PR_FREEBUSY_OOF_EVENTS			PROP_TAG(PT_MV_BINARY, 0x6856)
#define PR_FREEBUSY_OOF_MONTHS			PROP_TAG(PT_MV_LONG, 0x6855)
#define PR_FREEBUSY_START_RANGE			PROP_TAG(PT_LONG, 0x6847)
#define PR_FREEBUSY_TENTATIVE_EVENTS	PROP_TAG(PT_MV_BINARY, 0x6852)
#define PR_FREEBUSY_TENTATIVE_MONTHS	PROP_TAG(PT_MV_LONG, 0x6851)

#define PR_PERSONAL_FREEBUSY			PROP_TAG(PT_BINARY, 0x686C) // PR_FREEBUSY_DATA
//#define PR_RECALCULATE_FREEBUSY			PROP_TAG(PT_BOOLEAN, 0x10F2) PR_DISABLE_FULL_FIDELITY

//localfreebusy properties
#define PR_PROCESS_MEETING_REQUESTS					PROP_TAG(PT_BOOLEAN, 0x686D)
#define PR_DECLINE_CONFLICTING_MEETING_REQUESTS		PROP_TAG(PT_BOOLEAN, 0x686F)
#define PR_DECLINE_RECURRING_MEETING_REQUESTS		PROP_TAG(PT_BOOLEAN, 0x686E)  

#endif // FREEBUSYTAG_INCLUDED
