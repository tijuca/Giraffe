<?php
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

?>
<?

/**
 * Secondary state handling
 *
 * This class works exactly the same as standard PHP sessions files. We implement it here
 * so we can have improved granularity and don't have to put everything into the session
 * variables. Basically we have multiple session objects corresponding to multiple webclient
 * objects on the client. This class also locks the session files (in the same way as standard
 * PHP sessions), in order to serialize requests from the same client object.
 *
 * The main reason for creating this is to improve performance; normally PHP will lock the session
 * file for as long as your PHP request is running. However, we want to do multiple PHP requests at
 * once. For this reason, we only use the PHP session system for login information, and use this
 * class for other state information. This means that we can set a message as 'read' at the same time
 * as opening a dialog to view a message. 
 *
 * Currently, there is one 'state' for each 'subsystem'. The 'subsystem' is simply a tag which is appended
 * to the request URL when the client does an XML request. Each 'subsystem' has its own state file.
 *
 * Currently the subsystem is equal to the module ID. This means that if you have two requests from the same
 * module, they will have to wait for eachother. In practice this should hardly ever happen.
 *
 * @package core
 */

class State {

    var $fp;
    var $filename;
    
    var $sessiondir = "session";
    
    /**
     * @param string $subsystem Name of the subsystem
     */
    function State($subsystem) {
        $this->filename = TMP_PATH . "/" . $this->sessiondir . "/" . session_id() . "." . $subsystem;
    }
    
    /**
     * Open the session file
     *
     * The session file is opened and locked so that other processes can not access the state information
     */
    function open() {
        if(!is_dir(TMP_PATH . "/" . $this->sessiondir))
            mkdir(TMP_PATH . "/" . $this->sessiondir);
        $this->fp = fopen($this->filename, "a+");
        flock($this->fp, LOCK_EX);
    }
    
    /**
     * Read a setting from the state file
     *
     * @param string $name Name of the setting to retrieve
     * @return string Value of the state value, or null if not found
     * @todo The entire state file is read and parsed for each call of read()!
     */
    function read($name) {
        $contents = "";
        
        fseek($this->fp, 0);

        while($data = fread($this->fp, 32768)) 
            $contents .= $data;

        $data = unserialize($contents);

        if(isset($data[$name]))
            return $data[$name];
        else
            return false;
    }
    
    /**
     * Write a setting to the state file
     *
     * @param string $name Name of the setting to write
     * @param mixed $object Value of the object to be written to the setting
     * @todo The entire state file is read, parsed, modified and written for each call of write()!
     */
    function write($name, $object) {
        $contents = "";
        
        fseek($this->fp, 0);

        while($data = fread($this->fp, 32768)) 
            $contents .= $data;

        $data = unserialize($contents);
        $data[$name] = $object;
        
        $contents = serialize($data);
        
        ftruncate($this->fp, 0);
        fseek($this->fp, 0);
        fwrite($this->fp, $contents);
    }
 
    /**
     * Close the state file
     *
     * This closes and unlocks the state file so that other processes can access the state
     */
    function close() {
        if(isset($this->fp))
            fclose($this->fp);
    }
    
    /**
     * Cleans all old state information in the session directory
     */
    function clean() {
		cleanTemp(TMP_PATH . "/" . $this->sessiondir);
    }
}
