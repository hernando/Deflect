/*********************************************************************/
/* Copyright (c) 2013-2015, EPFL/Blue Brain Project                  */
/*                          Raphael Dumusc <raphael.dumusc@epfl.ch>  */
/*                          Daniel.Nachbaur@epfl.ch                  */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/*   1. Redistributions of source code must retain the above         */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer.                                                  */
/*                                                                   */
/*   2. Redistributions in binary form must reproduce the above      */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer in the documentation and/or other materials       */
/*      provided with the distribution.                              */
/*                                                                   */
/*    THIS  SOFTWARE IS PROVIDED  BY THE  UNIVERSITY OF  TEXAS AT    */
/*    AUSTIN  ``AS IS''  AND ANY  EXPRESS OR  IMPLIED WARRANTIES,    */
/*    INCLUDING, BUT  NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF    */
/*    MERCHANTABILITY  AND FITNESS FOR  A PARTICULAR  PURPOSE ARE    */
/*    DISCLAIMED.  IN  NO EVENT SHALL THE UNIVERSITY  OF TEXAS AT    */
/*    AUSTIN OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL,  SPECIAL, EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES    */
/*    (INCLUDING, BUT  NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE    */
/*    GOODS  OR  SERVICES; LOSS  OF  USE,  DATA,  OR PROFITS;  OR    */
/*    BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF    */
/*    LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY OUT    */
/*    OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF ADVISED  OF  THE    */
/*    POSSIBILITY OF SUCH DAMAGE.                                    */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of The University of Texas at Austin.                 */
/*********************************************************************/

#ifndef DEFLECT_SERVER_H
#define DEFLECT_SERVER_H

#include <deflect/api.h>
#include <deflect/types.h>
#include <deflect/SizeHints.h>

#include <QtNetwork/QTcpServer>

namespace deflect
{

/**
 * Listen to incoming PixelStream connections from Stream clients.
 */
class Server : public QTcpServer
{
    Q_OBJECT

public:
    /** The default port number used for Stream connections. */
    DEFLECT_API static const int defaultPortNumber;

    /** The zeroconf service name for announcing stream connections. */
    DEFLECT_API static const std::string serviceName;

    /**
     * Create a new server listening for Stream connections.
     * @param port The port to listen on. Must be available.
     * @throw std::runtime_error if the server could not be started.
     */
    DEFLECT_API explicit Server( int port = defaultPortNumber );

    /** Destructor */
    DEFLECT_API ~Server();

    /** Get the command handler. */
    DEFLECT_API CommandHandler& getCommandHandler();

    /** Get the PixelStreamDispatcher. */
    DEFLECT_API FrameDispatcher& getPixelStreamDispatcher();

signals:
    DEFLECT_API void registerToEvents( QString uri, bool exclusive,
                                       deflect::EventReceiver* receiver );

    DEFLECT_API void receivedSizeHints( QString uri, deflect::SizeHints hints );

public slots:
    DEFLECT_API void onPixelStreamerClosed( QString uri );
    DEFLECT_API void onEventRegistrationReply( QString uri, bool success );

private:
    class Impl;
    Impl* _impl;

    /** Re-implemented handling of connections from QTCPSocket. */
    void incomingConnection( qintptr socketHandle ) final;

signals:
    void _pixelStreamerClosed( QString uri );
    void _eventRegistrationReply( QString uri, bool success );
};

}

#endif
