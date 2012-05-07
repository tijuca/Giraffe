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

/**
 * Properties 
 * This contains the all the properties/labels to be used while creating particular 
 * item templates. If a property is listed here, the code will read that property 
 * from the item, and set it as label to the Templates
 */

function getMailProperties()
{
	var properties = new Object();
	properties["sent_representing_email_address"] = _("From");
	properties["message_delivery_time"] = _("Sent");
	properties["subject"] = _("Subject");
	properties["attachment"] = _("Attachment");
	properties["body"] = _("Body");

	return properties;
}

function getAppointmentProperties()
{
	var properties = new Object();
	properties["subject"] = _("Subject");
	properties["location"] = _("Location");
	properties["busystatus"] = _("Show Time As");
	properties["startdate"] = _("Start");
	properties["duedate"] = _("End");
	properties["recurring"] =_("Recurrence");
	properties["meeting"] = _("Meeting Status");
	properties["categories"] = _("Categories");
	properties["importance"] = _("Importance");
	properties["body"] = _("Body");

	return properties;
}


function getContactProperties()
{
	var properties = new Object();
	properties["display_name"] = _("Full Name");
	properties["surname"] =_("Last Name");
	properties["given_name"] = _("First Name");
	properties["title"] = _("Job Title");
	properties["department_name"] = _("Department");
	properties["company_name"] = _("Company")	
	//adress
	properties["business_address"] = _("Business Address");
	properties["home_address"] = _("Home Address");
	properties["other_address"] = _("Other Address");
	properties["im"] = _("IM Address");
	//phone
	properties["office_telephone_number"] = _("Business");
	properties["business2_telephone_number"] = _("Business2");
	properties["assistant_telephone_number"] = _("Assistant");
	properties["company_telephone_number"] = _("Company Main Phone");
	properties["home_telephone_number"] = _("Home");
	properties["home2_telephone_number"] = _("Home2");
	properties["cellular_telephone_number"] = _("Mobile");
	properties["car_telephone_number"] = _("Car");
	properties["radio_telephone_number"] = _("Radio");
	properties["pager_telephone_number"] = _("Pager");
	properties["callback_telephone_number"] = _("Callback");
	properties["other_telephone_number"] = _("Other");
	properties["primary_telephone_number"] = _("Primary Phone");
	properties["telex_telephone_number"] = _("Telex");
	properties["ttytdd_telephone_number"] = _("TTY/TDD Phone");
	properties["isdn_number"] = _("ISDN");
	properties["primary_fax_number"] = _("Primary Fax");
	properties["business_fax_number"] = _("Business Fax");
	properties["home_fax_number"] = _("Home Fax");
	//email address
	properties["email_address_1"] = _("E-mail");
	properties["email_address_display_name_1"] = _("E-mail Display As");
	properties["email_address_2"] = _("E-mail 2");
	properties["email_address_display_name_2"] = _("E-mail2 Display As");	
	properties["email_address_3"] = _("E-mail 3");
	properties["email_address_display_name_3"] = _("E-mail3 Display As");
	//details contatcs
	properties["birthday"] = _("Birthday");
	properties["wedding_anniversary"] = _("Anniversary");
	properties["spouse_name"] = _("Spouse/Partner");
	properties["profession"] = _("Profession");
	properties["assistant"] = _("Assistant");
	properties["webpage"] = _("Web Page");
	properties["categories"] = _("Categories");
	properties["body"] = _("Body");

	return properties;
}

function getTaskProperties()
{
	var properties = new Object();
	properties["subject"] = _("Subject");
	properties["importance"] = _("Priority");
	properties["startdate"] = _("Start Date");
	properties["duedate"] = _("End Date");
	properties["status"] = _("Status");
	properties["percent_complete"] = _("Percent Complete");
	properties["datecompleted"] = _("Date Completed");
	properties["totalwork"] = _("Total Work");
	properties["actualwork"] = _("Actual Work");
	properties["owner"] = _("Owner");
	properties["delegator"] = _("Requested By");
	properties["categories"] = _("Categories");
	properties["contacts"] = _("Contact");
	properties["companies"] = _("Company");
	properties["billinginformation"] = _("Bill Information");
	properties["mileage"] = _("Mileage");
	properties["body"] = _("Body");

	return properties;
}
