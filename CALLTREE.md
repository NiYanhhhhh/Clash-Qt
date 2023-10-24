### main
```
main->
    [MainWindow]->
        loadProfiles()
        [Tray] tray
    [Clash]->
        [RestfulApi] restfulApi
        [QProcess] process
        start()->
            process.start()->
            restfulApi.testConnection()->
                emit connected()->
                    MainWindow.onClashStarted()->
                        clash.api().listenTraffic()
                        proxyTimer.start()
                        configTimer.start()
                        onProfileChanged()

loadProfiles()->
    profile_list.append()
    profile_list.setIndex()
    useProfiles()->
        ui.profile.addItem(profile.name)
        ui.profile.setCurrentIndex()
```

### ProfileDialog
```

```

### signals
```
[MainWindow]:
    restfulApi.configUpdate->this.onConfigUpdate
    restfulApi.proxyDataReceived->this.updateProxies
    restfulApi.trafficUpdate->net_speed_label.setSpeed
    restfulApi.connected->this.onClashStarted
    QApplication::aboutToQuit->[this]{this->saveProfiles()}
[Tray]:
    restfulApi.errorHappened->onReceiveError
[Clash]:
    QApplication::aboutToQuit->this.stop
[RestfulApi]:
    proxyTimer.timeout->
        updateProxy->proxyDataReceived
    connectionTimer.timeout->
        updateConnection->connectionDataReceived
    configTimer.timeout->
        updateConfig->configUpdate
[ConnectionDialog]:
    timer.timeout->restfulApi.connectionDataReceived->
        this.updateConnection
```
