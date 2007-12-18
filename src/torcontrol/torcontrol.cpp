/****************************************************************
 *  Vidalia is distributed under the following license:
 *
 *  Copyright (C) 2006-2007,  Matt Edman, Justin Hipple
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

/** 
 * \file torcontrol.cpp
 * \version $Id$
 * \brief Object for interacting with the Tor process and control interface
 */

#include <QHostAddress>
#include <file.h>
#include <stringutil.h>
#include "torcontrol.h"


/** Default constructor */
TorControl::TorControl()
{
  /* Create an instance of a connection to Tor's control interface and give
   * it an object to use to handle asynchronous events. */
  _controlConn = new ControlConnection(&_torEvents);
  QObject::connect(_controlConn, SIGNAL(connected()),
                   this, SLOT(onConnected()));
  QObject::connect(_controlConn, SIGNAL(connectFailed(QString)),
                   this, SLOT(onConnectFailed(QString)));
  QObject::connect(_controlConn, SIGNAL(disconnected()),
                   this, SLOT(onDisconnected()));

  /* Create an object used to start and stop a Tor process. */
  _torProcess = new TorProcess(this);
  QObject::connect(_torProcess, SIGNAL(started()),
                   this, SLOT(onStarted()));
  QObject::connect(_torProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
                   this, SLOT(onStopped(int, QProcess::ExitStatus)));
  QObject::connect(_torProcess, SIGNAL(startFailed(QString)),
                   this, SLOT(onStartFailed(QString)));
  QObject::connect(_torProcess, SIGNAL(log(QString, QString)),
                   this, SLOT(onLogStdout(QString, QString)));

#if defined(Q_OS_WIN32)
  _torService = new TorService(this);
  QObject::connect(_torService, SIGNAL(started()), this, SLOT(onStarted()));
  QObject::connect(_torService, SIGNAL(finished(int, QProcess::ExitStatus)),
                   this, SLOT(onStopped(int, QProcess::ExitStatus)));
  QObject::connect(_torService, SIGNAL(startFailed(QString)),
                   this, SLOT(onStartFailed(QString))); 
#endif
}

/** Default destructor */
TorControl::~TorControl()
{
  /* Disconnect the control socket */
  if (isConnected()) {
    disconnect();
  }
  /* If we started our own Tor, stop it now */
  if (isVidaliaRunningTor()) {
    stop();
  }
  delete _controlConn;
}

/** Start the Tor process using the executable <b>tor</b> and the list of
 * arguments in <b>args</b>. */
void
TorControl::start(const QString &tor, const QStringList &args)
{
  if (isRunning()) {
    emit started();
  } else {
#if defined(Q_OS_WIN32)
    /* If the Tor service is installed, run that. Otherwise, start a new
     * Tor process. */
    if (TorService::isSupported() && _torService->isInstalled())
      _torService->start();
    else
      _torProcess->start(expand_filename(tor), args);
#else
    /* Start a new Tor process */
    _torProcess->start(expand_filename(tor), args);
#endif
  }
}

/** Emits a signal that the Tor process started */
void
TorControl::onStarted()
{
  emit started();
}

/** Emits a signal that the Tor process failed to start and includes an error
 * message (hopefully) indicating why. */
void
TorControl::onStartFailed(QString errmsg)
{
  emit startFailed(errmsg);
}

/** Stop the Tor process. */
bool
TorControl::stop(QString *errmsg)
{
  bool rc = false;
  if (_controlConn->isConnected())
    rc = signal(TorSignal::Halt, errmsg);
  if (!rc)
    rc = _torProcess->stop(errmsg);
  return rc;
}

/** Emits a signal that the Tor process stopped */
void
TorControl::onStopped(int exitCode, QProcess::ExitStatus exitStatus)
{
  if (_controlConn->status() == ControlConnection::Connecting)
    _controlConn->cancelConnect();
  
  emit stopped();
  emit stopped(exitCode, exitStatus);
}

/** Detects if the Tor process is running under Vidalia. Returns true if
 * Vidalia owns the Tor process, or false if it was an independent Tor. */
bool
TorControl::isVidaliaRunningTor()
{
  return (_torProcess->state() != QProcess::NotRunning);
}

/** Detect if the Tor process is running. */
bool
TorControl::isRunning()
{
  return (_torProcess->state() != QProcess::NotRunning
            || _controlConn->isConnected());
}

/** Called when Tor has printed a log message to stdout. */
void
TorControl::onLogStdout(QString severity, QString message)
{
  LogEvent::Severity s = LogEvent::toSeverity(severity);
  _torEvents.dispatch(TorEvents::toTorEvent(s), new LogEvent(s, message));
}

/** Connect to Tor's control port. The control port to use is determined by
 * Vidalia's configuration file. */
void
TorControl::connect(const QHostAddress &address, quint16 port)
{
  _controlConn->connect(address, port);
}

/** Emits a signal that the control socket successfully established a
 * connection to Tor. */
void
TorControl::onConnected()
{
  /* Let interested parties know that the control socket connected */
  emit connected();
}

/** Emits a signal that the control connection to Tor failed. */
void
TorControl::onConnectFailed(QString errmsg)
{
  emit connectFailed(errmsg);
}

/** Disconnect from Tor's control port */
void
TorControl::disconnect()
{
  if (isConnected())
    _controlConn->disconnect();
}

/** Emits a signal that the control socket disconnected from Tor */
void
TorControl::onDisconnected()
{
  if (_torProcess) {
    /* If we're running a Tor process, then start reading logs from stdout
     * again, in case our control connection just died but Tor is still
     * running. In this case, there may be relevant information in the logs. */ 
    _torProcess->openStdout();
  }
  /* Tor isn't running, so it has no version */
  _torVersion = QString();

  /* Let interested parties know we lost our control connection */
  emit disconnected();
}

/** Check if the control socket is connected */
bool
TorControl::isConnected()
{
  return _controlConn->isConnected();
}

/** Send a message to Tor and reads the response. If Vidalia was unable to
 * send the command to Tor or read its response, false is returned. If the
 * response was read and the status code is not 250 OK, false is also
 * returned. */
bool
TorControl::send(ControlCommand cmd, ControlReply &reply, QString *errmsg)
{
  if (_controlConn->send(cmd, reply, errmsg)) {
    if (reply.getStatus() == "250") {
      return true;
    }
    if (errmsg) {
      *errmsg = reply.getMessage();
    }
  }
  return false;
}

/** Sends a message to Tor and discards the response. */
bool
TorControl::send(ControlCommand cmd, QString *errmsg)
{
  ControlReply reply;
  return send(cmd, reply, errmsg);
}

/** Sends an authentication cookie to Tor. The syntax is:
 * 
 *   "AUTHENTICATE" SP 1*HEXDIG CRLF
 */
bool
TorControl::authenticate(const QByteArray cookie, QString *errmsg)
{
  ControlCommand cmd("AUTHENTICATE", base16_encode(cookie));
  ControlReply reply;
  QString str;
  
  if (!send(cmd, reply, &str)) {
    emit authenticationFailed(str);
    return err(errmsg, str);
  }
  onAuthenticated(); 
  return true;
}

/** Sends an authentication password to Tor. The syntax is:
 * 
 *   "AUTHENTICATE" SP QuotedString CRLF
 */
bool
TorControl::authenticate(const QString password, QString *errmsg)
{
  ControlCommand cmd("AUTHENTICATE", QString("%1")
                                      .arg(string_escape(password)));
  ControlReply reply;
  QString str;
  
  if (!send(cmd, reply, &str)) {
    emit authenticationFailed(str);
    return err(errmsg, str);
  }
  onAuthenticated(); 
  return true;
}

/** Called when the controller has successfully authenticated to Tor. */
void
TorControl::onAuthenticated()
{
  /* The version of Tor isn't going to change while we're connected to it, so
   * save it for later. */
  getInfo("version", _torVersion);
  
  /* The control socket is connected, so we can stop reading from stdout */
  if (_torProcess)
    _torProcess->closeStdout();
  
  emit authenticated();
}

/** Sends a PROTOCOLINFO command to Tor and parses the response. */
ProtocolInfo
TorControl::protocolInfo(QString *errmsg)
{
  ControlCommand cmd("PROTOCOLINFO", "1");
  ControlReply reply;
  ProtocolInfo pi;
  QString msg, topic;
  QHash<QString,QString> keyvals;
  int idx;
  bool ok;

  if (!send(cmd, reply, errmsg))
    return ProtocolInfo();

  foreach (ReplyLine line, reply.getLines()) {
    if (line.getStatus() != "250")
      continue;
    
    msg = line.getMessage().trimmed();
    idx = msg.indexOf(" ");
    topic = msg.mid(0, idx).toUpper();
    
    if (idx > 0) {
      keyvals = string_parse_keyvals(msg.mid(idx+1), &ok);
      if (!ok)
        continue; /* Ignore malformed lines */
    } else {
      keyvals = QHash<QString,QString>();
    }
   
    if (topic == "AUTH") {
      if (keyvals.contains("METHODS"))
        pi.setAuthMethods(keyvals.value("METHODS"));
      if (keyvals.contains("COOKIEFILE"))
        pi.setCookieAuthFile(keyvals.value("COOKIEFILE"));
    } else if (topic == "VERSION") {
      if (keyvals.contains("Tor"))
        pi.setTorVersion(keyvals.value("Tor"));
    }
  }
  return pi;
}

/** Returns true if Tor either has an open circuit or (on Tor >=
 * 0.2.0.1-alpha) has previously decided it's able to establish a circuit. */
bool
TorControl::circuitEstablished()
{
  /* If Tor is recent enough, we can 'getinfo status/circuit-established' to
   * see if Tor has an open circuit */
  if (getTorVersion() >= 0x020001) {
    QString tmp;
    if (getInfo("status/circuit-established", tmp))
      return (tmp == "1");
  }

  /* Either Tor was too old or our getinfo failed, so try to get a list of all
   * circuits and check their statuses. */
  QList<Circuit> circs = getCircuits();
  foreach (Circuit circ, circs) {
    if (circ.status() == Circuit::Built)
      return true;
  }
  return false;
}

/** Sends a GETINFO message to Tor based on the given map of keyvals. The
 * syntax is:
 * 
 *    "GETINFO" 1*(SP keyword) CRLF 
 */
bool
TorControl::getInfo(QHash<QString,QString> &map, QString *errmsg)
{
  ControlCommand cmd("GETINFO");
  ControlReply reply;

  /* Add the keys as arguments to the GETINFO message */
  foreach (QString key, map.keys()) {
    cmd.addArgument(key);
  }
 
  /* Ask Tor for the specified info values */
  if (send(cmd, reply, errmsg)) {
    /* Parse the response for the returned values */
    foreach (ReplyLine line, reply.getLines()) {
      /* Split the "key=val" line and map them */
      QStringList keyval = line.getMessage().split("=");
      if (keyval.size() == 2) {
        map.insert(keyval.at(0), keyval.at(1));
      }
    }
    return true;
  }
  return false;
}

/** Overloaded method to send a GETINFO command for a single info value */
bool
TorControl::getInfo(QString key, QString &val, QString *errmsg)
{
  QHash<QString,QString> map;
  map.insert(key, "");

  if (getInfo(map, errmsg)) {
    val = map.value(key);
    return true;
  }
  return false;
}

/** Sends a signal to Tor */
bool
TorControl::signal(TorSignal::Signal sig, QString *errmsg)
{
  ControlCommand cmd("SIGNAL");
  cmd.addArgument(TorSignal::toString(sig));

  if (sig == TorSignal::Shutdown || sig == TorSignal::Halt) {
    /* Tor closes the connection before giving us a response to any commands
     * asking it to stop running, so don't try to get a response. */
    return _controlConn->send(cmd, errmsg);
  }
  return send(cmd, errmsg); 
}

/** Returns an address on which Tor is listening for application
 * requests. If none are available, a null QHostAddress is returned. */
QHostAddress
TorControl::getSocksAddress(QString *errmsg)
{
  QHostAddress socksAddr;

  /* If SocksPort is 0, then Tor is not accepting any application requests. */
  if (getSocksPort() == 0) {
    return QHostAddress::Null;
  }
  
  /* Get a list of SocksListenAddress lines and return the first valid IP
   * address parsed from the list. */
  QStringList addrList = getSocksAddressList(errmsg);
  foreach (QString addr, addrList) {
    addr = addr.mid(0, addr.indexOf(":"));
    if (socksAddr.setAddress(addr)) {
      return socksAddr;
    }
  }
  /* Otherwise Tor is listening on its default 127.0.0.1 */
  return QHostAddress::LocalHost;
}

/** Returns a (possibly empty) list of all currently configured 
 * SocksListenAddress entries. */
QStringList
TorControl::getSocksAddressList(QString *errmsg)
{
  QStringList addrList;
  if (getConf("SocksListenAddress", addrList, errmsg)) {
    return addrList;
  }
  return QStringList();
}

/** Returns a valid SOCKS port for Tor, or 0 if Tor is not accepting
 * application requests. */
quint16
TorControl::getSocksPort(QString *errmsg)
{
  QList<quint16> portList = getSocksPortList(errmsg);
  if (portList.size() > 0) {
    return portList.at(0);
  }
  return 0;
}

/** Returns a list of all currently configured SOCKS ports. If Tor is not
 * accepting any application connections, an empty list will be returned. */
QList<quint16>
TorControl::getSocksPortList(QString *errmsg)
{
  bool valid;
  quint16 port, socksPort;
  QString portString;
  QList<quint16> portList;
 
  /* Get the value of the SocksPort configuration variable */ 
  if (getConf("SocksPort", portString, errmsg)) {
    socksPort = (quint16)portString.toUInt(&valid);
    if (valid) {
      if (socksPort == 0) {
        /* A SocksPort of 0 means Tor is not accepting any application
         * connections. */
        return QList<quint16>();
      }
    }
  }
  /* Get a list of SOCKS ports from SocksListenAddress entries */
  QStringList addrList = getSocksAddressList(errmsg);
  foreach (QString addr, addrList) {
    if (addr.contains(":")) {
      portString = addr.mid(addr.indexOf(":")+1);
      port = (quint16)portString.toUInt(&valid);
      if (valid) {
        portList << port;
      }
    }
  }
  /* If there were no SocksListenAddress entries, or one or more of them did
   * not specify a port, then add the value of SocksPort, too */
  if (!portList.size() || (portList.size() != addrList.size())) {
    portList << socksPort;
  }
  return portList;
}

/** Reeturns Tor's version as a string. */
QString
TorControl::getTorVersionString()
{
  return _torVersion;
}

/** Returns Tor's version as a numeric value. Note that this discards any
 * version status flag, such as "-alpha" or "-rc". */
quint32
TorControl::getTorVersion()
{
  static QString versionString;
  static quint32 version = 0;
  quint8 major, minor, micro, patch;

  /* Only recompute the version number if the version string changed */
  if (versionString == _torVersion)
    return version;
  versionString = _torVersion;

  /* Split the version string at either "." or "-" characters */
  QStringList parts = versionString.split(QRegExp("\\.|-"));
  if (parts.size() >= 4) {
    major = (quint8)parts.at(0).toUInt();
    minor = (quint8)parts.at(1).toUInt();
    micro = (quint8)parts.at(2).toUInt();
    patch = (quint8)parts.at(3).toUInt();
    version = ((major << 24) | (minor << 16) | (micro << 8) | patch);
  } else {
    /* Couldn't parse the version string */
    version = 0;
  }
  return version;
}

/** Sets an event and its handler. If add is true, then the event is added,
 * otherwise it is removed. If set is true, then the given event will be
 * registered with Tor. */
bool
TorControl::setEvent(TorEvents::TorEvent e, QObject *obj, 
                     bool add, bool set, QString *errmsg)
{
  if (add) {
    _torEvents.add(e, obj);
  } else {
    _torEvents.remove(e, obj);
  }
  if (set && isConnected()) {
    return setEvents(errmsg);
  }
  return true;
}

/** Registers for a set of logging events according to the given filter. If
 * the control socket is currently connected, this method will try to register
 * the log events with Tor, otherwise it will simply return true. */
bool
TorControl::setLogEvents(uint filter, QObject *obj, QString *errmsg)
{
  setEvent(TorEvents::LogError , obj, filter & LogEvent::Error , false);
  setEvent(TorEvents::LogWarn  , obj, filter & LogEvent::Warn  , false);
  setEvent(TorEvents::LogNotice, obj, filter & LogEvent::Notice, false);
  setEvent(TorEvents::LogInfo  , obj, filter & LogEvent::Info  , false);
  setEvent(TorEvents::LogDebug , obj, filter & LogEvent::Debug , false);
  return (isConnected() ? setEvents(errmsg) : true);
}

/** Register for the events currently in the event list */
bool
TorControl::setEvents(QString *errmsg)
{
  ControlCommand cmd("SETEVENTS"); 

  /* Add each event to the argument list */
  foreach (TorEvents::TorEvent e, _torEvents.eventList()) {
    cmd.addArgument(TorEvents::toString(e));
  }
  return send(cmd, errmsg);
}

/** Sets each configuration key in <b>map</b> to the value associated 
 * with its key. */
bool
TorControl::setConf(QHash<QString,QString> map, QString *errmsg)
{
  ControlCommand cmd("SETCONF");
  
  /* Add each keyvalue to the argument list */
  foreach (QString key, map.uniqueKeys()) {
    foreach (QString value, map.values(key)) {
      if (value.length() > 0)
        cmd.addArgument(key + "=" + string_escape(value));
      else
        cmd.addArgument(key);
    }
  }
  return send(cmd, errmsg); 
}

/** Sets a single configuration key to the given value. */
bool
TorControl::setConf(QString key, QString value, QString *errmsg)
{
  QHash<QString,QString> map;
  map.insert(key, value);
  return setConf(map, errmsg);
}

/** Gets values for a set of configuration keys, each of which has a single
 * value. */
bool
TorControl::getConf(QHash<QString,QString> &map, QString *errmsg)
{
  QHash<QString,QStringList> multiMap;
  foreach (QString key, map.keys()) {
    multiMap.insert(key, QStringList());
  }
  if (getConf(multiMap, errmsg)) {
    foreach (QString key, multiMap.keys()) {
      if (map.contains(key)) {
        map.insert(key, multiMap.value(key).join("\n"));
      }
    }
  }
  return false;
}

/** Gets a set of configuration keyvalues and stores them in <b>map</b>. */
bool
TorControl::getConf(QHash<QString,QStringList> &map, QString *errmsg)
{
  ControlCommand cmd("GETCONF");
  ControlReply reply;
  QStringList confValue;
  QString confKey;

  /* Add the keys as arguments to the GETINFO message */
  foreach (QString key, map.keys()) {
    cmd.addArgument(key);
  }

  /* Ask Tor for the specified info values */
  if (send(cmd, reply, errmsg)) {
    /* Parse the response for the returned values */
    foreach (ReplyLine line, reply.getLines()) {
      /* Split the "key=val" line and map them */
      QStringList keyval = line.getMessage().split("=");
      if (keyval.size() == 2) {
        confKey = keyval.at(0);
       
        if (map.contains(confKey)) {
          /* This configuration key has multiple values, so add this one to
           * the list. */
          confValue = map.value(confKey);
        }
        confValue << keyval.at(1);
        map.insert(keyval.at(0), confValue);
      }
    }
    return true;
  }
  return false;
}

/** Gets a single configuration value for <b>key</b>. */
bool
TorControl::getConf(QString key, QString &value, QString *errmsg)
{
  QStringList confValues;
  if (getConf(key, confValues, errmsg)) {
    value = confValues.join("\n");
    return true;
  }
  return false;
}

/** Gets a list of configuration values for <b>key</b>. */
bool
TorControl::getConf(QString key, QStringList &value, QString *errmsg)
{
  QHash<QString,QStringList> map;
  map.insert(key, QStringList());

  if (getConf(map, errmsg)) {
    value = map.value(key);
    return true;
  }
  return false;
}

/** Asks Tor to save the current configuration to its torrc. */
bool
TorControl::saveConf(QString *errmsg)
{
  ControlCommand cmd("SAVECONF");
  return send(cmd, errmsg);
}

/** Tells Tor to reset the given configuration keys back to defaults. */
bool
TorControl::resetConf(QStringList keys, QString *errmsg)
{
  ControlCommand cmd("RESETCONF");

  /* Add each key to the argument list */
  foreach (QString key, keys) {
    cmd.addArgument(key);
  }
  return send(cmd, errmsg);
}

/** Tells Tor to reset a single given configuration key back to its default
 * value. */
bool
TorControl::resetConf(QString key, QString *errmsg)
{
  return resetConf(QStringList() << key, errmsg);
}

/** Gets the descriptor for the specified router name. */
RouterDescriptor
TorControl::getDescriptorByName(QString name, QString *errmsg)
{
  QList<RouterDescriptor> rdlist;
  RouterDescriptor rd;
  
  rdlist = getDescriptorListByName(QStringList() << name, errmsg);
  if (!rdlist.isEmpty()) {
    rd = (RouterDescriptor)rdlist.takeFirst();
    return rd;
  }
  return RouterDescriptor();
}

/** Gets the descriptor for the specified ID. */
RouterDescriptor
TorControl::getDescriptorById(QString id, QString *errmsg)
{
  QList<RouterDescriptor> rdlist;
  RouterDescriptor rd;
  
  rdlist = getDescriptorListById(QStringList() << id, errmsg);
  if (!rdlist.isEmpty()) {
    rd = (RouterDescriptor)rdlist.takeFirst();
    return rd;
  }
  return RouterDescriptor();
}

/** Gets router descriptors for all names in <b>nameList</b>. */
QList<RouterDescriptor>
TorControl::getDescriptorListByName(QStringList nameList, QString *errmsg)
{
  ControlCommand cmd("GETINFO");
  ControlReply reply;
  QList<RouterDescriptor> rdlist;
  
  /* If there are no IDs in the list, then return now. */
  if (nameList.isEmpty()) {
    return QList<RouterDescriptor>();
  }
  
  /* Build up the the getinfo arguments from the list of names */
  foreach (QString name, nameList) {
    cmd.addArgument("desc/name/"+ name);
  }
  
  /* Request the list of router descriptors */
  if (send(cmd, reply, errmsg)) {
    foreach (ReplyLine line, reply.getLines()) {
      /* Check if we got a "250 OK" and descriptor data. */
      if (line.getStatus() == "250" && !line.getData().isEmpty()) {
        /* Parse the router descriptor data */
        rdlist << RouterDescriptor(line.getData());
      }
    }
  }
  return rdlist;
}

/** Gets router descriptors for all IDs in <b>idlist</b>. */
QList<RouterDescriptor>
TorControl::getDescriptorListById(QStringList idlist, QString *errmsg)
{
  ControlCommand cmd("GETINFO");
  ControlReply reply;
  QHash<QString,bool> offline;
  QList<RouterDescriptor> rdlist;
  
  /* If there are no IDs in the list, then return now. */
  if (idlist.isEmpty()) {
    return QList<RouterDescriptor>();
  }
  
  /* Build up the the getinfo arguments fromthe list of IDs */
  foreach (QString id, idlist) {
    if (id.startsWith("!")) {
      /* The ! means this router is offline. Save a list of the offline
       * routers for easy lookup after we get all descriptors from Tor */
      id = id.remove("!");
      offline.insert(id, true);
    }
    cmd.addArgument("desc/id/"+ id);
  }
  
  /* Request the list of router descriptors */
  if (send(cmd, reply, errmsg)) {
    foreach (ReplyLine line, reply.getLines()) {
      /* Check if we got a "250 OK" and descriptor data. */
      if (line.getStatus() == "250" && !line.getData().isEmpty()) {
        /* Parse the router descriptor data */
        RouterDescriptor rd(line.getData());
        rd.setOffline(!offline.isEmpty() && offline.contains(rd.id()));
        rdlist << rd;
      }
    }
  }
  return rdlist;
}

/** Gets a list of RouterDescriptor objects for all routers that Tor currently
 * knows about. */
QList<RouterDescriptor>
TorControl::getRouterList(QString *errmsg)
{
  /* Get a list of all router IDs Tor currently know about */
  QStringList idList = getRouterIDList(errmsg);
  /* Get descriptors for each of those routers */
  return getDescriptorListById(idList, errmsg);
}

/** Gets a list of router IDs for all routers Tor knows about. */
QStringList
TorControl::getRouterIDList(QString *errmsg)
{
  ControlCommand cmd("GETINFO", "network-status");
  ControlReply reply;
  QStringList idList;

  if (send(cmd, reply, errmsg)) {
    QString routerIDs = reply.getMessage().remove(0,qstrlen("network-status="));

    /* Split the list of router IDs up */
    QStringList routers = routerIDs.split(" ");
    foreach (QString router, routers) {
      /* A router ID may be of the form <name>=$<ID>, $<ID>, or <ID> */
      QString id = router.mid(router.indexOf("=")+1);
      id = id.replace("$", "");
      /* A "!" before <name> or <ID> means "unresponsive" */
      if (router.startsWith("!") && !id.startsWith("!")) {
        id.prepend("!");
      }
      /* Add this router ID to the list. */
      idList << id;
    }
  }
  return idList;
}

/** Gets a list of current circuits. */
QList<Circuit>
TorControl::getCircuits(QString *errmsg)
{
  ControlCommand cmd("GETINFO", "circuit-status");
  ControlReply reply;
  QList<Circuit> circuits;
  Circuit c;
  
  if (send(cmd, reply, errmsg)) {
    /* Sometimes there is a circuit on the first message line */
    QString msg = reply.getMessage();
    c = Circuit::fromString(msg.mid(msg.indexOf("=")+1));
    if (!c.isEmpty()) {
      circuits << c;
    }

    /* The rest of the circuits just come as data, one per line */
    foreach(QString line, reply.getData()) {
      c = Circuit::fromString(line);
      if (!c.isEmpty()) {
        circuits << Circuit::fromString(line);
      }
    }
  }
  return circuits;
}

/** Closes the circuit specified by <b>circid</b>. If <b>ifUnused</b> is
 * true, then the circuit will not be closed unless it is unused. */
bool
TorControl::closeCircuit(quint64 circid, bool ifUnused, QString *errmsg)
{
  ControlCommand cmd("CLOSECIRCUIT", QString::number(circid));
  if (ifUnused) {
    cmd.addArgument("IfUnused");
  }
  return send(cmd, errmsg);
}

/** Gets a list of current streams. */
QList<Stream>
TorControl::getStreams(QString *errmsg)
{
  ControlCommand cmd("GETINFO", "stream-status");
  ControlReply reply;
  QList<Stream> streams;
  Stream s;
  
  if (send(cmd, reply, errmsg)) {
    /* Sometimes there is a stream on the first message line */
    QString msg = reply.getMessage();
    s = Stream::fromString(msg.mid(msg.indexOf("=")+1));
    if (!s.isEmpty()) {
      streams << s;
    }
    
    /* The rest of the streams jsut come as data, one per line */
    foreach (QString line, reply.getData()) {
      s = Stream::fromString(line);
      if (!s.isEmpty()) {
        streams << s;
      }
    }
  }
  return streams;
}

/** Closes the stream specified by <b>streamid</b>. */
bool
TorControl::closeStream(quint64 streamid, QString *errmsg)
{
  ControlCommand cmd("CLOSESTREAM", QString::number(streamid));
  cmd.addArgument("1"); /* 1 == REASON_MISC (tor-spec.txt) */
  return send(cmd, errmsg);
}

 /** Gets a list of address mappings of the type specified by <b>type</b>
  * (defaults to <i>AddressMapAll</i>. */
AddressMap
TorControl::getAddressMap(AddressMap::AddressMapType type, QString *errmsg)
{
  ControlCommand cmd("GETINFO");
  ControlReply reply;
  AddressMap addressMap;

  switch (type) {
    case AddressMap::AddressMapConfig:
      cmd.addArgument("addr-mappings/config");
      break;
    case AddressMap::AddressMapCache:
      cmd.addArgument("addr-mappings/cache");
      break;
    case AddressMap::AddressMapControl:
      cmd.addArgument("addr-mappings/control");
      break;
    default:
      cmd.addArgument("addr-mappings/all");
  }

  if (send(cmd, reply, errmsg)) {
    foreach (QString mapping, reply.getData()) {
      addressMap.add(mapping);
    }
  }
  return addressMap;
}
