#ifndef HELPERPROCESSPROT_H
#define HELPERPROCESSPROT_H

#include <QtGui>
#include <QtScript>

#include "HelperProcess.h"

class HelperProcessPrototype : public QObject, public QScriptable
{
  Q_OBJECT

  public:
    HelperProcessPrototype(QObject *parent = 0);
    static QScriptValue constructor(QScriptContext *context, QScriptEngine *engine);
    static int metaTypeId();
    static QString name();

    Q_INVOKABLE void start(const QString &app, const QString &args);
    Q_INVOKABLE void start(const QString &app, const QStringList &args);
    Q_INVOKABLE bool isDone() const;
};

Q_DECLARE_METATYPE(HelperProcess *);

#endif

