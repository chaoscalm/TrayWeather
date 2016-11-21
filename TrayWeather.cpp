/*
 File: TrayWeather.cpp
 Created on: 13/11/2016
 Author: Felix de las Pozas Alvarez

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Project
#include <TrayWeather.h>
#include <AboutDialog.h>

// Qt
#include <QNetworkReply>
#include <QObject>
#include <QMenu>
#include <QApplication>
#include <QDesktopWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QDebug>

//--------------------------------------------------------------------
TrayWeather::TrayWeather(const Configuration& configuration, QObject* parent)
: QSystemTrayIcon{parent}
, m_configuration{configuration}
, m_netManager   {std::make_shared<QNetworkAccessManager>()}
, m_timer        {this}
{
  setIcon(QIcon{":/TrayWeather/application.ico"});
  m_timer.setSingleShot(true);

  connectSignals();

  createMenuEntries();

  requestForecastData();
}

//--------------------------------------------------------------------
void TrayWeather::replyFinished(QNetworkReply* reply)
{
  if(reply->error() == QNetworkReply::NoError)
  {
    auto type = reply->header(QNetworkRequest::ContentTypeHeader).toString();

    if(type.startsWith("application/json"))
    {
      auto jsonDocument = QJsonDocument::fromJson(reply->readAll());

      if(!jsonDocument.isNull() && jsonDocument.isObject())
      {
        m_data.clear();

        auto jsonObj = jsonDocument.object();
        auto values  = jsonObj.value("list").toArray();

        for(auto i = 0; i < values.count(); ++i)
        {
          auto obj     = values.at(i).toObject();
          auto main    = obj.value("main").toObject();
          auto weather = obj.value("weather").toArray().first().toObject();
          auto wind    = obj.value("wind").toObject();
          auto rain    = obj.value("rain").toObject();
          auto snow    = obj.value("snow").toObject();

          ForecastData data;
          data.dt          = obj.value("dt").toInt(0);
          data.cloudiness  = obj.value("clouds").toObject().value("all").toDouble(0);
          data.temp        = main.value("temp").toDouble(0);
          data.temp_min    = main.value("temp_min").toDouble(0);
          data.temp_max    = main.value("temp_max").toDouble(0);
          data.humidity    = main.value("humidity").toDouble(0);
          data.pressure    = main.value("grnd_level").toDouble(0);
          data.description = weather.value("description").toString();
          data.parameters  = weather.value("main").toString();
          data.icon_id     = weather.value("icon").toString();
          data.weather_id  = weather.value("id").toDouble(0);
          data.wind_speed  = wind.value("speed").toDouble(0);
          data.wind_dir    = wind.value("deg").toDouble(0);
          data.snow        = snow.keys().contains("3h") ? snow.value("3h").toDouble(0) : 0;
          data.rain        = rain.keys().contains("3h") ? rain.value("3h").toDouble(0) : 0;

          m_data << data;
        }
      }
    }

    if(!m_data.isEmpty())
    {
      setIcon(QIcon{":/TrayWeather/application.ico"});
      m_timer.setInterval(m_configuration.updateTime*60*1000);
      m_timer.start();
    }

    reply->deleteLater();
    return;
  }

  // change to error icon.
  setIcon(QIcon{":/TrayWeather/network_error.ico"});

  reply->deleteLater();
}

//--------------------------------------------------------------------
void TrayWeather::createMenuEntries()
{
  auto menu = new QMenu(nullptr);

  auto weather = new QAction{QIcon{":/TrayWeather/temp-celsius.svg"}, tr("Forecast..."), menu};
  connect(weather, SIGNAL(triggered(bool)), this, SLOT(showForecast()));

  menu->addSeparator();

  auto about = new QAction{QIcon{":/TrayWeather/information.svg"}, tr("About..."), menu};
  connect(about, SIGNAL(triggered(bool)), this, SLOT(showAboutDialog()));

  menu->addAction(about);

  auto exit = new QAction{QIcon{":/TrayWeather/exit.svg"}, tr("Quit"), menu};
  connect(exit, SIGNAL(triggered(bool)), this, SLOT(exitApplication()));

  menu->addAction(exit);

  this->setContextMenu(menu);
}

//--------------------------------------------------------------------
void TrayWeather::exitApplication()
{
  this->hide();

  QApplication::instance()->quit();
}

//--------------------------------------------------------------------
void TrayWeather::showAboutDialog() const
{
  static AboutDialog dialog;

  if(!dialog.isVisible())
  {
    auto scr = QApplication::desktop()->screenGeometry();
    dialog.move(scr.center() - dialog.rect().center());
    dialog.setModal(true);

    dialog.exec();
  }
}

//--------------------------------------------------------------------
void TrayWeather::connectSignals()
{
  connect(m_netManager.get(), SIGNAL(finished(QNetworkReply*)),
          this,               SLOT(replyFinished(QNetworkReply*)));

  connect(this, SIGNAL(messageClicked()),
          this, SLOT(onMessageClicked()));

  connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
          this, SLOT(onActivation(QSystemTrayIcon::ActivationReason)));

  connect(&m_timer, SIGNAL(timeout()),
          this,     SLOT(requestForecastData()));
}

//--------------------------------------------------------------------
void TrayWeather::onMessageClicked()
{
  this->showMessage(QString(), QString(), QSystemTrayIcon::MessageIcon::NoIcon, 0);
}

//--------------------------------------------------------------------
void TrayWeather::onActivation(QSystemTrayIcon::ActivationReason reason)
{
  if(reason == QSystemTrayIcon::ActivationReason::DoubleClick)
  {
    // TODO: show forecast.
    showForecast();
  }
}

//--------------------------------------------------------------------
void TrayWeather::showForecast() const
{
}

//--------------------------------------------------------------------
void TrayWeather::requestForecastData()
{
  setIcon(QIcon{":/TrayWeather/network_refresh.ico"});
  m_timer.setInterval(1*60*1000);
  m_timer.start();

  auto url = QUrl{QString(tr("http://api.openweathermap.org/data/2.5/forecast?lat=%1&lon=%2&appid=%3").arg(m_configuration.latitude)
                                                                                                      .arg(m_configuration.longitude)
                                                                                                      .arg(m_configuration.owm_apikey))};
  m_netManager->get(QNetworkRequest{url});
}
