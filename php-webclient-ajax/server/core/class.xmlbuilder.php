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
<?php
	/**
	* XML serialiser class
	*
	* This class builds an XML string. It receives an associative array, which 
	* will be converted to a XML string. The data in the associative array is
	* assumed to hold UTF-8 strings, and the output XML is output as UTF-8 also;
	* no charset processing is performed.
	*
	* @package core
	*/
	
	class XMLBuilder
	{
		/**
		 * @var string this string the builded XML
		 */	
		var $xml;
		
		/**
		 * @var integer this variable is used for indenting (DEBUG)
		 */ 		 		
		var $depth;
		
		function XMLBuilder()
		{
			if(defined("DEBUG_XML_INDENT") && DEBUG_XML_INDENT) {
			    $this->indent = true;
            } else {
                $this->indent = false;
            }
		} 

		/**
		 * Builds the XML using the given associative array
		 * @param array $data The data which should be converted to a XML string
		 * @return string The serialised XML string
		 */		 		
		function build($data)
		{
			$this->depth = 1;
			$this->xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?" . ">\n<zarafa>\n";
			$this->addData($data);
			$this->xml .= "</zarafa>";
			return $this->xml;
		}

		/**
		 * Add data to serialised XML string
		 *
		 * The type of data is autodetected and forwarded to the correct add*() function
		 *
		 * @access private
		 * @param array $data array of data which should be converted to a XML string		 
		 */ 		 				
		function addData($data)
		{
			if(is_array($data)) {
				foreach($data as $tagName => $tagValue)
				{
					if(is_assoc_array($tagValue)) {
						$this->addAssocArray($tagName, $tagValue);
					} else if(is_array($tagValue)) {
						$this->addArray($tagName, $tagValue);
					} else {
						$this->addNode($tagName, $tagValue);
					}
				}
			}
		}

		/**
		 * Adds a non-associated array to the XML string.
		 * @access private
		 * @param string $parentTag the parent tag of all items in the array
		 * @param array $data array of data
		 */ 		 				
		function addArray($parentTag, $data)
		{
			foreach($data as $tagValue)
			{
				$this->addAssocArray($parentTag, $tagValue);
			}
		}

		/**
		 * Adds an associative array to the XML string.
		 * @access private
		 * @param string $parentTag the parent tag		 
		 * @param array $data array of data 		 
		 */ 		 				
		function addAssocArray($parentTag, $data)
		{
			$attributes = $this->getAttributes($data);
			if($this->indent)
    			$this->xml .= str_repeat("\t",$this->depth);
			$this->xml .=  "<" . $parentTag . $attributes . ">";

			if(is_array($data)) {

				if(isset($data["_content"])) {
					if (!is_array($data["_content"])){
						$this->xml .= xmlentities($data["_content"]);
					}else{
						$this->depth++; 
						if($this->indent)
    						$this->xml .= "\n";
						$this->addData($data["_content"]);
						$this->depth--;
						if($this->indent)
    						$this->xml .= str_repeat("\t",$this->depth);
					}
				} else {
					$this->depth++; 
                    if($this->indent)
    					$this->xml .= "\n";
					$this->addData($data);
					$this->depth--;
					if($this->indent)
						$this->xml .= str_repeat("\t",$this->depth);
				}
			} else {
				$this->xml .= xmlentities($data);
			}
				
			$this->xml .= "</" . $parentTag . ">";
			if($this->indent)
			    $this->xml .= "\n";
		}
		
		/**
		 * Add a node to the XML string.
		 * @access private
		 * @param string $tagName the tag		 
		 * @param string $value the value of the tag 		 
		 */
		function addNode($tagName, $value)
		{
		    if($this->indent)
    			$this->xml .= str_repeat("\t",$this->depth);

			/**
			 * When converting a float value into a string PHP replaces the decimal point into a 
			 * comma if the language settings are set to a language that uses the comma as a decimal
			 * separator. This check makes sure only a decimal point is used in the XML.
			 */
			if(is_float($value)){
				$value = str_replace(',','.',strval($value));
			}

			$this->xml .= "<" . $tagName . ">" . xmlentities($value) . "</" . $tagName . ">";
			if($this->indent)
			    $this->xml .= "\n";
		}
		
		/**
		 * Verify if there are any attributes in the given array.
		 * It returns a string with the attributes, which will be added to the
		 * XML string.
		 * @access private
		 * @param array $data the array which be checked
		 * @return string a string with the attributes		  		 		 		 
		 */		 		
		function getAttributes(&$data)
		{
			$attributes = "";
			
			if(isset($data["attributes"]) && is_array($data["attributes"])) {
				$attributes = "";
				
				foreach($data["attributes"] as $attribute => $value)
				{
					$attributes .= " ".$attribute . "=\"" . xmlentities($value) . "\"";
				}
				
				unset($data["attributes"]);
			}
			
			return $attributes;
		}
	}
?>
