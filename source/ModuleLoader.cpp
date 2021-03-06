#include "ModuleLoader.hpp"

#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QLibrary>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include <QtCore/QDebug>

#include <QtWidgets/QApplication>

#include "Creator.hpp"
#include "ComponentCreatorSet.hpp"
#include "FunctionCall.hpp"

#include <iostream>

namespace ComponentManager {

inline
QString
constructSharedLibraryPath(QString const &moduleName,
                           QString const &modulePath)
{
  QDir appPath(QCoreApplication::applicationDirPath());

  appPath.cd(appPath.absoluteFilePath(modulePath));

  return appPath.filePath(moduleName);
}


inline
QJsonObject
readJsonFile(QString const &jsonFile)
{
  if (!QFileInfo::exists(jsonFile))
  {
    qDebug() << "File "
             << jsonFile
             << " does not exist.";

    return QJsonObject();
  }

  QFile file(jsonFile);

  if (!file.open(QIODevice::ReadOnly))
    return QJsonObject();

  QByteArray wholeFileData = file.readAll();

  QJsonParseError parseError;

  QJsonObject const jsonDocument =
    QJsonDocument::fromJson(wholeFileData, &parseError).object();

  Q_ASSERT_X(parseError.error == QJsonParseError::NoError,
             "ComponentManager::loadModule",
             parseError.errorString().toUtf8().constData());

  return jsonDocument;
}


inline
QJsonObject
readModuleJson(QJsonObject const & jsonDocument)
{
  QJsonObject moduleJson = jsonDocument["module"].toObject();

  Q_ASSERT_X(!jsonDocument.isEmpty(),
             "ComponentManager::loadModule",
             "Empty module json is loaded.");

  return moduleJson;
}


inline
void
loadSharedLibrary(QJsonObject const &moduleJson)
{
  QString moduleName = moduleJson["name"].toString();

  Q_ASSERT_X(!moduleName.isEmpty(),
             "ComponentManager::loadModule",
             "Module name is empty");

  // 2.1 Module's shared library path (relative to executable)

  QString modulePath = moduleJson["path"].toString();

  Q_ASSERT_X(!modulePath.isEmpty(),
             "ComponentManager::loadModule",
             "Module path is empty");

  // 3. Path to the shared library

  QString sharedLibPath = constructSharedLibraryPath(moduleName, modulePath);

  // 4. Load shared library

  QLibrary module(sharedLibPath);

  QFunctionPointer componentRegistratorFunction =
    module.resolve("registerComponent");

  Q_ASSERT_X(QFileInfo::exists(module.fileName()),
             "ComponentManager::loadModule",
             "Shared library does not exist.");

  QString cantResolve =
    QString("Cannot resolve function 'registerComponent()' in library:\n %1").arg(sharedLibPath);

  Q_ASSERT_X(componentRegistratorFunction != nullptr,
             "ModuleLoader",
             sharedLibPath.toUtf8().constData());

  // Register!
  componentRegistratorFunction();
}



inline
void
loadOneModule(QString const &moduleJsonFile,
              std::vector<FunctionCall> & functionCalls)
{
  QJsonObject jsonDocument = readJsonFile(moduleJsonFile);

  QJsonObject moduleJson = readModuleJson(jsonDocument);

  loadSharedLibrary(moduleJson);

  // Parse declared Components and add them to the global creator

  Creator::merge(ComponentManager::createComponentCreatorSet(moduleJson));

  auto fc = loadFunctionCalls(moduleJson);

  std::move(std::make_move_iterator(fc.begin()),
            std::make_move_iterator(fc.end()),
            std::back_inserter(functionCalls));
}


void
loadModules(std::vector<QString> const &moduleJsonFiles)
{
  std::vector<FunctionCall> functionCalls;

  for (auto const & s : moduleJsonFiles)
  {
    loadOneModule(s, functionCalls);
  }

  invokeFunctionCalls(functionCalls);
}


void
loadModules(QString const &moduleJsonFile)
{
  std::vector<FunctionCall> functionCalls;

  loadOneModule(moduleJsonFile, functionCalls);

  invokeFunctionCalls(functionCalls);
}
}
