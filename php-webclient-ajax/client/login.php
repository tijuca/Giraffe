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
	$user = htmlentities($_GET["user"]);
	header("Content-type: text/html; charset=utf-8");
?><!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
	<head>
		<title>Zarafa WebAccess</title>
		<link rel="stylesheet" type="text/css" href="client/layout/css/login.css">
		<link rel="icon" href="client/layout/img/favicon.ico"  type="image/x-icon">
		<link rel="shortcut icon" href="client/layout/img/favicon.ico" type="image/x-icon">	
		<script type="text/javascript">
			window.onload = function(){
				if (document.getElementById("username").value == ""){
					document.getElementById("username").focus();
				}else if (document.getElementById("password").value == ""){
					document.getElementById("password").focus();
				}
			}
		</script>
	</head>
	<body class="login">
		<table id="layout">
			<tr><td>
				<div id="login_main">
					<form action="index.php?logon<?=($user)?'&user='.$user:''?>" method="post">
					<!-- Store action attributes to hidden variable to pass it to index page -->
					<?php	if($_POST && $_POST["action_url"] != "") {	?>
								<!-- if login has failed then action attributes will be in POST variable -->
								<input type="hidden" name="action_url" value="<?= htmlspecialchars($_POST["action_url"]) ?>"></input>
					<?php	} else {	?>
								<!-- or else in the URL -->
								<input type="hidden" name="action_url" value="<?= stristr($_SERVER["REQUEST_URI"], "?action=") ?>"></input>
					<?php	}	?>
					
						<div id="login_data">
							<p><?=_("Please logon")?>.</p>
							<p class="error"><?php

	if (isset($_SESSION) && isset($_SESSION["hresult"])) {
		switch($_SESSION["hresult"]){
			case MAPI_E_LOGON_FAILED:
			case MAPI_E_UNCONFIGURED:
				echo _("Logon failed, please check your name/password.");
				break;
			case MAPI_E_NETWORK_ERROR:
				echo _("Cannot connect to the Zarafa Server.");
				break;
			default:
				echo "Unknown MAPI Error: ".get_mapi_error_name($_SESSION["hresult"]);
		}
		unset($_SESSION["hresult"]);
	}else if (isset($_GET["logout"]) && $_GET["logout"]=="auto"){
		echo _("You have been automatically logged out");
	}else{
		echo "&nbsp;";
	}
							?></p>
							<table id="form_fields">
								<tr>
									<th><label for="username"><?=_("Name")?>:</label></th>
									<td><input type="text" name="username" id="username" class="inputelement"
									<?php
									 if (defined("CERT_VAR_TO_COMPARE_WITH") && $_SERVER ) {
										echo " value='".$_SERVER[CERT_VAR_TO_COMPARE_WITH]."' readonly='readonly'";
									 } elseif ($user) {
									 	echo " value='".$user."'";
									 }
									?>></td>
								</tr>
								<tr>
									<th><label for="password"><?=_("Password")?>:</label></th>
									<td><input type="password" name="password" id="password" class="inputelement"></td>
								</tr>
								<tr>
									<th><label for="language"><?=_("Language")?>:</label></th>
									<td>
										<select name="language" id="language" class="inputelement">
											<option value="last"><?=_("Last used language")?></option>
<?php
  function langsort($a, $b) { return strcasecmp($a, $b); } 	
  $langs = $GLOBALS["language"]->getLanguages();
  uasort($langs, 'langsort');
  foreach($langs as $lang=>$title){ 
?>											<option value="<?=$lang?>"><?=$title?></option>
<?php   } ?>
										</select>
									</td>
								</tr>
								<tr>
									<td>&nbsp;</td>
									<td><input id="submitbutton" type="submit" value=<?=_("Logon")?>></td>
								</tr>
							</table>
						</div>
					</form>
					<span id="version"><?=defined("DEBUG_SERVER_ADDRESS")?"Server: ".DEBUG_SERVER_ADDRESS." - ":""?><?=phpversion("mapi")?><?=defined("SVN")?"-svn".SVN:""?></span>
				</div>
			</td></tr>
		</table>
	</body>
</html>
