<?php

/**
 * Copyright (C) 2014 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

require_once "openmediavault/error.inc";
require_once "openmediavault/notify.inc";
require_once "openmediavault/object.inc";
require_once "openmediavault/rpc.inc";
require_once "openmediavault/util.inc";

class OMVRpcServiceNginxServer extends OMVRpcServiceAbstract
{
    const HOST_TYPE_PORT = "port";
    const HOST_TYPE_NAME = "name";

    private $logDir = "/var/log/nginx";

    private function getEventMessagePath()
    {
        return "org.openmediavault.services.nginx";
    }

    private function getPhpFpmEventMessagePath()
    {
        return "org.openmediavault.services.phpfpm";
    }

    private function getXpath()
    {
        return "//services/nginx";
    }

    private function getServersXpath()
    {
        return sprintf("%s/servers", $this->getXpath());
    }

    private function getServerXpathByUuid($uuid)
    {
        return sprintf("%s/server[uuid='%s']", $this->getServersXpath(), $uuid);
    }

    public function getName()
    {
        return "Nginx";
    }

    public function initialize()
    {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod("getList");

        // CRUD
        $this->registerMethod("get");
        $this->registerMethod("set");
        $this->registerMethod("delete");

        // Log
        $this->registerMethod("getLog");
    }

    public function getSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get configuration object
        $object = $xmlConfig->get($this->getXpath());
        if (is_null($object)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $this->getXpath());
        }

        // Modify result data
        $settingsObject["enable"]                        = boolval($object["enable"]);
        $settingsObject["server_names_hash_bucket_size"] = intval($object["server_names_hash_bucket_size"]);

        return $settingsObject;
    }

    public function setSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "enable"                        : { "type" : "boolean" },
                    "server_names_hash_bucket_size" : { "type" : "integer", "enum" : [ 32, 64, 128 ] }
                }
            }'
        );

        // Prepare configuration data
        $object = array(
            "enable"                        => array_boolval($params, "enable"),
            "server_names_hash_bucket_size" => $params["server_names_hash_bucket_size"]
        );

        // Get old configuration object
        $oldConfig = $xmlConfig->get($this->getXpath());
        if (is_null($object)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $this->getXpath());
        }

        $object["servers"] = $oldConfig["servers"];

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXpath(), $object)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED, $this->$xpath);
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->getEventMessagePath(),
            $object
        );
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->getPhpFpmEventMessagePath()
        );

        return $object;
    }

    public function getList($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "start"     : { "type" : "integer" },
                    "limit"     : {' . $GLOBALS["OMV_JSONSCHEMA_COUNTFIELD"] . '},
                    "sortfield" : {' . $GLOBALS["OMV_JSONSCHEMA_SORTFIELD"] . '},
                    "sortdir"   : {' . $GLOBALS["OMV_JSONSCHEMA_SORTDIR"] . '}
                }
            }'
        );

        // Get configuration object
        $xpath = sprintf("%s/server/uuid", $this->getServersXpath());
        $uuids = $xmlConfig->getList($xpath);

        if (is_null($uuids)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
        }

        $servers = array();

        foreach ($uuids as $uuid) {
            try {
                $servers[] = $this->get(array("uuid" => $uuid), $context);
            } catch (Exception $e) {
                throw $e;
            }
        }

        return $this->applyFilter(
            $servers,
            $params["start"],
            $params["limit"],
            $params["sortfield"],
            $params["sortdir"]
        );
    }

    public function get($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "uuid" : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_NONE"] . '}
                }
            }'
        );

        $server = $xmlConfig->get($this->getServerXpathByUuid($params["uuid"]));

        if (is_null($server)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getServerXpathByUuid($params["uuid"])
            );
        }

        // General
        $server["enable"]               = boolval($server["enable"]);
        $server["use_root"]             = boolval($server["use_root"]);
        $server["use_public_directory"] = boolval($server["use_public_directory"]);
        $server["port"]                 = intval($server["port"]);
        $server["port_default_server"]  = boolval($server["port_default_server"]);

        // SSL
        $server["ssl_enable"]              = boolval($server["ssl_enable"]);
        $server["ssl_port"]                = intval($server["ssl_port"]);
        $server["ssl_port_default_server"] = boolval($server["ssl_port_default_server"]);
        $server["ssl_force"]               = boolval($server["ssl_force"]);

        // PHP
        $server["php_enable"]             = boolval($server["php_enable"]);
        $server["php_use_default_config"] = boolval($server["php_use_default_config"]);

        // Options
        $server["use_index"]                   = boolval($server["use_index"]);
        $server["use_index_html"]              = boolval($server["use_index_html"]);
        $server["use_index_php"]               = boolval($server["use_index_php"]);
        $server["autoindex"]                   = boolval($server["autoindex"]);
        $server["deny_htaccess"]               = boolval($server["deny_htaccess"]);
        $server["log_enable"]                  = boolval($server["log_enable"]);
        $server["large_client_header_buffers"] = intval($server["large_client_header_buffers"]);

        // If ports is missing a value, just set default value
        if (!$server["port"]) {
            $server["port"] = 80;
        }

        if (!$server["ssl_port"]) {
            $server["ssl_port"] = 443;
        }

        // Get full path for presentation
        $rootFullPath = null;

        if (!empty($server["sharedfolderref"])) {
            $rootFullPath = $this->getSharedFolderPathByUuid(
                $server["sharedfolderref"]
            );

            if ($server["use_root"] && $server["use_public_directory"]) {
                $rootFullPath = $rootFullPath
                              . DIRECTORY_SEPARATOR
                              . $server["public_directory"];
            }
        }

        $server["root_full_path"] = $rootFullPath;

        // Build possible URLs
        $urls = array();
        $serverNames = array();

        if ($server["host_type"] == self::HOST_TYPE_PORT) {
            $serverNames[] = "!domain!";
        } else {
            $serverNames = explode(" ", $server["server_name"]);
        }

        foreach ($serverNames as $domain) {
            if (!$server["ssl_force"]) {
                $urls[] = sprintf("http://%s:%s", $domain, $server["port"]);
            }

            if ($server["ssl_enable"]) {
                $urls[] = sprintf("https://%s:%s", $domain, $server["ssl_port"]);
            }
        }

        $server["urls"] = $urls;

        return $server;
    }

    public function set($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "uuid"                 : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_UNDEFINED"] . '},
                    "enable"               : { "type" : "boolean" },
                    "sharedfolderref"      : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_NONE"] . '},
                    "use_root"    : { "type" : "boolean" },
                    "use_public_directory" : { "type" : "boolean" },
                    "public_directory"     : { "type" : "string", "optional" : true },
                    "host_type"            : { "type" : "string", "enum" : [ "port", "name" ] },
                    "server_name"          : { "type" : "string" },
                    "port"                 : { "type" : "integer" },
                    "port_default_server"  : { "type" : "boolean" },

                    "ssl_enable"              : { "type" : "boolean" },
                    "ssl_port"                : { "type" : "integer" },
                    "ssl_port_default_server" : { "type" : "boolean" },
                    "sslcertificateref"       : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_NONE"] . '},
                    "ssl_force"               : { "type" : "boolean" },

                    "php_enable"             : { "type" : "boolean" },
                    "php_pool_ref"           : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_NONE"] . '},
                    "php_use_default_config" : { "type" : "boolean" },

                    "use_index"                   : { "type" : "boolean" },
                    "use_index_html"              : { "type" : "boolean" },
                    "use_index_php"               : { "type" : "boolean" },
                    "autoindex"                   : { "type" : "boolean" },
                    "deny_htaccess"               : { "type" : "boolean" },
                    "log_enable"                  : { "type" : "boolean" },
                    "large_client_header_buffers" : { "type" : "integer", "enum" : [ 8, 16, 32 ]},

                    "extra_options" : { "type" : "string" }
                }
            }'
        );

        $server = array(
            "uuid" =>
                ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) ?
                OMVUtil::uuid() : $params["uuid"],
            "enable"               => boolval($params["enable"]),
            "sharedfolderref"      => $params["sharedfolderref"],
            "use_root"             => boolval($params["use_root"]),
            "use_public_directory" => boolval($params["use_public_directory"]),
            "public_directory"     => trim(trim($params["public_directory"]), DIRECTORY_SEPARATOR),
            "host_type"            => $params["host_type"],
            "server_name"          => $params["server_name"],
            "port"                 => intval($params["port"]),
            "port_default_server"  => boolval($params["port_default_server"]),

            "ssl_enable"              => boolval($params["ssl_enable"]),
            "ssl_port"                => intval($params["ssl_port"]),
            "ssl_port_default_server" => boolval($params["ssl_port_default_server"]),
            "sslcertificateref"       => $params["sslcertificateref"],
            "ssl_force"               => boolval($params["ssl_force"]),

            "php_enable"              => boolval($params["php_enable"]),
            "php_pool_ref"            => $params["php_pool_ref"],
            "php_use_default_config"  => boolval($params["php_use_default_config"]),

            "use_index"                   => boolval($params["use_index"]),
            "use_index_html"              => boolval($params["use_index_html"]),
            "use_index_php"               => boolval($params["use_index_php"]),
            "autoindex"                   => boolval($params["autoindex"]),
            "deny_htaccess"               => boolval($params["deny_htaccess"]),
            "log_enable"                  => boolval($params["log_enable"]),
            "large_client_header_buffers" => intval($params["large_client_header_buffers"]),

            "extra_options" => trim($params["extra_options"]),
        );

        $server = $this->sanitizeServer($server);
        $server = $this->validateServer($server);

        // Save configuration
        $success = false;
        $xpath = null;

        if ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) {
            $success = $xmlConfig->set(
                $xpath = $this->getServersXpath(),
                array("server" => $server),
                $server
            );
        } else {
            $success = $xmlConfig->replace(
                $xpath = $this->getServerXpathByUuid($params["uuid"]),
                $server
            );
        }

        if (!$success) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_OBJECT_NOT_FOUND, $xpath);
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            $params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"] ?
            OMV_NOTIFY_CREATE : OMV_NOTIFY_MODIFY,
            $this->getEventMessagePath(),
            $server
        );

        return $server;
    }

    public function delete($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "uuid" : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_NONE"] . '}
                }
            }'
        );

        $server = $xmlConfig->delete($this->getServerXpathByUuid($params["uuid"]));

        if (is_null($server)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_NOT_FOUND,
                $this->getServerXpathByUuid($params["uuid"])
            );
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_DELETE, $this->getEventMessagePath(), $server);
    }

    public function getLog($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "start"     : { "type" : "integer" },
                    "limit"     : {' . $GLOBALS["OMV_JSONSCHEMA_COUNTFIELD"] . '},
                    "sortfield" : {' . $GLOBALS["OMV_JSONSCHEMA_SORTFIELD"] . '},
                    "sortdir"   : {' . $GLOBALS["OMV_JSONSCHEMA_SORTDIR"] . '},
                    "uuid"      : {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '},
                    "type"      : { "type" : "string", "enum" : [ "access", "error" ]}
                }
            }'
        );

        $uuid = $params["uuid"];
        $type = $params["type"];

        $logRows = array();

        $logPath = sprintf(
            "%s/%s-%s.log",
            $this->logDir,
            $uuid,
            $type
        );

        if (file_exists($logPath)) {
            $logText = file_get_contents($logPath);

            $logTextArray = explode("\n", $logText);
            $rowNumber = 1;

            foreach ($logTextArray as $row) {
                if (!empty($row)) {
                    $logRows[] = array(
                        "id"      => $rowNumber++,
                        "message" => $row,
                    );
                }
            }
        }

        return array(
            "total" => count($logRows),
            "data"  => $logRows,
        );
    }

    private function sanitizeServer($server)
    {
        // Trim whitespace and then trim / at start and end
        $server["public_directory"] = trim($server["public_directory"]);
        $server["public_directory"] = trim($server["public_directory"], DIRECTORY_SEPARATOR);

        return $server;
    }

    private function validateServer($server)
    {
        // Remove current port number if not used
        if ($server["ssl_force"]) {
            $server["port"] = null;
        }

        if (!$server["ssl_enable"]) {
            $server["ssl_port"] = null;
        }

        // Perform port checks
        if ($server["port"] == $server["ssl_port"]) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_INVALID,
                "Ports identical, can't use same port for SSL."
            );
        }

        if (!$this->validatePort($server["port"], $server["uuid"], $server["host_type"])) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_INVALID,
                "Selected port is already used"
            );
        }

        if (!$this->validatePort($server["ssl_port"], $server["uuid"], $server["host_type"], true)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_INVALID,
                "Selected port (SSL) is already used"
            );
        }

        // Validate the default server attribute
        if (!$server["ssl_force"]) {
            if ($server["port_default_server"]) {
                if (!$this->validatePortDefaultServer($server["port"], $server["uuid"])) {
                    throw new OMVException(
                        OMVErrorMsg::E_CONFIG_OBJECT_INVALID,
                        "There is already another server set as the default on the selected port."
                    );
                }
            }
        } else {
            $server["port_default_server"] = false;
        }

        if ($server["ssl_enable"]) {
            if ($server["ssl_port_default_server"]) {
                if (!$this->validatePortDefaultServer($server["ssl_port"], $server["uuid"], true)) {
                    throw new OMVException(
                        OMVErrorMsg::E_CONFIG_OBJECT_INVALID,
                        "There is already another server set as the default on the selected SSL port."
                    );
                }
            }
        } else {
            $server["ssl_port_default_server"] = false;
        }

        // Trim and replace all spaces with a single space
        // to allow exploding in the get method
        $serverName = $server["server_name"];
        $serverName = trim($serverName);
        $serverName = preg_replace("/\s+/", " ", $serverName);
        $server["server_name"] = $serverName;

        return $server;
    }

    private function validatePort($port, $uuid, $hostType, $sslPort = false)
    {
        global $xmlConfig;

        $xpath = sprintf("%s/server[port='%s' and ssl_force='0']", $this->getServersXpath(), $port);
        $server = $xmlConfig->get($xpath);

        $sslXPath = sprintf("%s/server[ssl_port='%s' and ssl_enable='1']", $this->getServersXpath(), $port);
        $sslServer = $xmlConfig->get($sslXPath);

        // If not empty and not the same as
        // the current one, check if port types
        // mismatches and then if any of them
        // are port based
        if (!empty($server)) {
            if ($server["uuid"] != $uuid) {
                if ($sslPort) {
                    return false;
                }

                if ($hostType == self::HOST_TYPE_PORT || $server["host_type"] == self::HOST_TYPE_PORT) {
                    return false;
                }
            } else {
                if ($sslPort) {
                    return false;
                }
            }
        }

        if (!empty($sslServer)) {
            if ($sslServer["uuid"] != $uuid) {
                if (!$sslPort) {
                    return false;
                }

                if ($hostType == self::HOST_TYPE_PORT || $server["host_type"] == self::HOST_TYPE_PORT) {
                    return false;
                }
            } else {
                if (!$sslPort) {
                    return false;
                }
            }
        }

        // Validate with webadmin ports
        $webadminConfig = $xmlConfig->get("//webadmin");

        if (!empty($webadminConfig)) {
            $webadminPort    = intval($webadminConfig["port"]);
            $webadminSslPort = intval($webadminConfig["sslport"]);
            $forceSslOnly    = boolval($webadminConfig["forcesslonly"]);
            $enableSsl       = boolval($webadminConfig["enablessl"]);

            if ($webadminPort == $port && !$forceSslOnly && $server["host_type"] == self::HOST_TYPE_PORT) {
	        // name based port sharing is allowed in nginx
                return false;
            }

            if ($webadminSslPort == $port && $enableSsl && $server["host_type"] == self::HOST_TYPE_PORT) {
	        // name based port sharing is allowed in nginx
                return false;
            }
        }

        return true;
    }

    private function validatePortDefaultServer($port, $uuid, $ssl = false)
    {
        global $xmlConfig;
        $xpath = "";

        if ($ssl) {
            $xpath = sprintf(
                "%s/server[ssl_port='%s' and ssl_port_default_server='1' and uuid!='%s']",
                $this->getServersXpath(),
                $port,
                $uuid
            );
        } else {
            $xpath = sprintf(
                "%s/server[port='%s' and port_default_server='1' and uuid!='%s']",
                $this->getServersXpath(),
                $port,
                $uuid
            );
        }

        return $xmlConfig->count($xpath) === 0;
    }

    private function getSharedFolderPathByUuid($uuid)
    {
        global $xmlConfig;

        $xpath = sprintf("//system/shares/sharedfolder[uuid='%s']", $uuid);
        $sharedFolder = $xmlConfig->get($xpath);
        $sharedFolderRelDirPath = trim($sharedFolder["reldirpath"], DIRECTORY_SEPARATOR);

        $xpath = sprintf("//system/fstab/mntent[uuid='%s']", $sharedFolder["mntentref"]);
        $mountPoint = $xmlConfig->get($xpath);
        $mountPointDir = rtrim($mountPoint["dir"], DIRECTORY_SEPARATOR);

        return $mountPointDir . DIRECTORY_SEPARATOR . $sharedFolderRelDirPath;
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceNginxServer());
