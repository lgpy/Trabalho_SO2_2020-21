# TODO
## control

- [x] single instance
- [x] read registry for max avioes and max aeroportos
- [x] set registry if not set
- [ ] shared memory thread doesnt stop because waiting for writes

## aviao

- [x] DLL import
- [ ] get seats, speed and initial airport from console
- [ ] shared memory thread doesnt stop because waiting for writes

## communication
- [ ] get AirportCoords


## ideas

thread do aviao é controlada por um evento no main()
eg: obter coords ao inicio, aciona-se o evento e a thread envia quando terminar acabar o evento.

aviao gera um mapa com nome "mapa-" + PId
control guarda esse mapa no dados->Aviao
usa isso para responder e envia um evento?