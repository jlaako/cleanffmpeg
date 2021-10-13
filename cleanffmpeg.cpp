/*

  Cleans ffmpeg source tree from unused components.
  Copyright (C) 2021 Jussi Laako.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/


#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>


static QStringList disabledList;
static QStringList enabledList;
static QStringList removeList;
static QStringList keepList;


static QStringList getObjs (const QString &lineP, const QString &oper)
{
    QString line(lineP);

    int op = line.indexOf(oper);
    if (op > 0)
        line.remove(0, op + 2);
    else
        qDebug("error: operator missing!");

    QStringList objList;
    QStringList itemList(line.split(' ', QString::SkipEmptyParts));
    foreach (QString item, itemList)
    {
        QString obj(item.trimmed());
        if (obj.endsWith(QStringLiteral(".o")))
        {
            qDebug("\tobject: %s", obj.toUtf8().constData());
            objList.append(obj);
        }
    }

    return objList;
}


static void processDir (const QString &dirName)
{
    QDir dir(dirName);

    qDebug("process %s", dirName.toUtf8().constData());
    QStringList dirList(dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot));

    foreach (const QString &dirItem, dirList)
        processDir(dirName + QStringLiteral("/") + dirItem);

    QString makefileName(dirName + QStringLiteral("/Makefile"));
    if (!QFile::exists(makefileName))
        return;
    if (!QFile::copy(makefileName, makefileName + QStringLiteral(".old")))
    {
        qDebug("error copying Makefile");
        return;
    }
    QFile makefileOld(makefileName + QStringLiteral(".old"));
    if (!makefileOld.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug("failed to open Makefile.old");
        return;
    }
    QFile makefileNew(makefileName);
    if (!makefileNew.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug("failed to open Makefile");
        QFile::remove(makefileName + QStringLiteral(".old"));
        return;
    }

    int escape;
    QString line;

    while (!makefileOld.atEnd())
    {
        escape = 0;
        line.clear();

        do {
            QString sub(makefileOld.readLine());
            escape = sub.indexOf('\\');
            if (escape >= 0)
                line += sub.left(escape);
            else
                line += sub;
        } while ((escape >= 0) && (!makefileOld.atEnd()));

        if (line.startsWith(QStringLiteral("OBJS-")))
        {
            int start = line.indexOf('(');
            if (start < 0)
            {
                makefileNew.write(line.toUtf8());
                continue;
            }
            start++;
            int end = line.indexOf(')', start);
            QString variable(line.mid(start, end - start));
            if (!variable.startsWith(QStringLiteral("CONFIG_")))
            {
                makefileNew.write(line.toUtf8());
                continue;
            }
            if (disabledList.contains(variable))
            {
                qDebug("disabled variable: %s", variable.toUtf8().constData());
                removeList.append(getObjs(line, QStringLiteral("+=")));
            }
            else if (enabledList.contains(variable))
            {
                qDebug("enabled variable: %s", variable.toUtf8().constData());
                keepList.append(getObjs(line, QStringLiteral("+=")));
                makefileNew.write(line.toUtf8());
            }
            else
                makefileNew.write(line.toUtf8());
        }
        else if (line.startsWith(QStringLiteral("OBJS")))
        {
            keepList.append(getObjs(line, QStringLiteral("=")));
            makefileNew.write(line.toUtf8());
        }
        else
            makefileNew.write(line.toUtf8());
    }

    foreach (QString obj, removeList)
    {
        if (keepList.contains(obj))
            continue;

        QString fileName;
        QString filePath;

        fileName = obj;
        fileName.replace(QStringLiteral(".o"), QStringLiteral(".c"));
        filePath = dirName + QStringLiteral("/") + fileName;
        if (QFile::exists(filePath))
        {
            qDebug("remove: %s", filePath.toUtf8().constData());
            QFile::remove(filePath);
        }

        fileName = obj;
        fileName.replace(QStringLiteral(".o"), QStringLiteral(""));
        QStringList subFilter;
        subFilter.append(fileName + QStringLiteral("_*.c"));
        subFilter.append(fileName + QStringLiteral("*data*.h"));
        QStringList subList(dir.entryList(QStringList(fileName), QDir::Files));
        foreach (const QString &entry, subList)
        {
            filePath = dirName + QStringLiteral("/") + entry;
            qDebug("remove: %s", filePath.toUtf8().constData());
            QFile::remove(filePath);
        }

        // NOTE: various headers are still needed, you may need to copy back
        fileName = obj;
        fileName.replace(QStringLiteral(".o"), QStringLiteral(".h"));
        filePath = dirName + QStringLiteral("/") + fileName;
        if (QFile::exists(filePath))
        {
            qDebug("remove: %s", filePath.toUtf8().constData());
            QFile::remove(filePath);
        }

        fileName = obj;
        fileName.replace(QStringLiteral(".o"), QStringLiteral(".asm"));
        filePath = dirName + QStringLiteral("/") + fileName;
        if (QFile::exists(filePath))
        {
            qDebug("remove: %s", filePath.toUtf8().constData());
            QFile::remove(filePath);
        }

        fileName = obj;
        fileName.replace(QStringLiteral(".o"), QStringLiteral(".S"));
        filePath = dirName + QStringLiteral("/") + fileName;
        if (QFile::exists(filePath))
        {
            qDebug("remove: %s", filePath.toUtf8().constData());
            QFile::remove(filePath);
        }
    }

    // some additional custom cleanup
    QStringList subFilter;

    /*subFilter.append(QStringLiteral("h26*.c"));
    subFilter.append(QStringLiteral("h26*.h"));
    subFilter.append(QStringLiteral("h26*.asm"));
    subFilter.append(QStringLiteral("h26*.S"));
    subFilter.append(QStringLiteral("ac3*.c"));
    subFilter.append(QStringLiteral("ac3*.h"));
    subFilter.append(QStringLiteral("ac3*.asm"));
    subFilter.append(QStringLiteral("ac3*.S"));*/
    subFilter.append(QStringLiteral("adts*.c"));
    subFilter.append(QStringLiteral("adts*.h"));
    subFilter.append(QStringLiteral("adts*.asm"));
    subFilter.append(QStringLiteral("adts*.S"));
    subFilter.append(QStringLiteral("wma*"));
    subFilter.append(QStringLiteral("g7*"));
    subFilter.append(QStringLiteral("gsm*"));
    subFilter.append(QStringLiteral("vp*"));
    subFilter.append(QStringLiteral("aac*"));
    subFilter.append(QStringLiteral("rv*"));
    subFilter.append(QStringLiteral("vc1*"));
    //subFilter.append(QStringLiteral("*h26*"));

    QStringList subList(dir.entryList(subFilter, QDir::Files));
    foreach (const QString &entry, subList)
    {
        QString filePath = dirName + QStringLiteral("/") + entry;
        qDebug("remove: %s", filePath.toUtf8().constData());
        QFile::remove(filePath);
    }

    makefileOld.remove();
}


int main (int argc, char *argv[])
{
    if (argc < 2)
        return 1;

    QCoreApplication app(argc, argv);
    QString baseDir(QString::fromUtf8(argv[1], -1));

    QFile fileConfig(baseDir + QStringLiteral("/ffbuild/config.mak"));
    if (!fileConfig.exists())
    {
        qDebug("ffmpeg tree needs to be configured first!");
        return 1;
    }
    if (!fileConfig.open(QIODevice::ReadOnly | QIODevice::Text))
        return 1;
    while (!fileConfig.atEnd())
    {
        QString line(fileConfig.readLine());

        if (line.startsWith(QStringLiteral("!CONFIG_")))
        {
            line.remove(0, 1);
            line.truncate(line.indexOf('='));
            qDebug("disabled: %s", line.toUtf8().constData());
            disabledList.append(line);
        }
        else if (line.startsWith(QStringLiteral("CONFIG_")))
        {
            line.truncate(line.indexOf('='));
            qDebug("enabled: %s", line.toUtf8().constData());
            enabledList.append(line);
        }
    }

    processDir(QString::fromUtf8(argv[1], -1));

    return 0;
}

