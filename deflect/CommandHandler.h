/*********************************************************************/
/* Copyright (c) 2014, EPFL/Blue Brain Project                       */
/*                     Raphael Dumusc <raphael.dumusc@epfl.ch>       */
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

#ifndef DEFLECT_COMMANDHANDLER_H
#define DEFLECT_COMMANDHANDLER_H

#include <deflect/api.h>
#include <deflect/types.h>
#include <deflect/CommandType.h>

#include <QObject>

namespace deflect
{

/**
 * A command handler for String formatted network commands.
 */
class CommandHandler : public QObject
{
    Q_OBJECT

public:
    /** Constructor */
    DEFLECT_API CommandHandler();

    /** Destructor */
    DEFLECT_API ~CommandHandler();

    /**
     * Register a command handler.
     *
     * This class takes ownership of the handler. If a handler for the same
     * CommandType was already registered, it will be replaced.
     * @param handler The handler to register.
     */
    DEFLECT_API void registerCommandHandler( AbstractCommandHandler* handler );

    /**
     * Unregister a command handler.
     *
     * @param type The type for which to unregister the command handler.
     */
    DEFLECT_API void unregisterCommandHandler( CommandType type );

public slots:
    /**
     * Process a command.
     * @param command The command string, formatted by the Command class.
     * @param parentWindowUri Identifier of the window issuing the command.
     */
    DEFLECT_API void process( QString command, QString parentWindowUri );

private:
    typedef std::map< CommandType, AbstractCommandHandler* > CommandHandlerMap;
    CommandHandlerMap _handlers;
};

}

#endif
