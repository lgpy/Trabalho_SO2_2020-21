# TODO
## control

- [x] single instance
- [x] read registry for max avioes and max aeroportos
- [x] set registry if not set

## aviao

- [x] DLL import
- [x] get seats, speed and initial airport from console
- [ ] Se a posi��o estiver ocupada, o avi�o dever� lidar com a situa��o de forma coerente (desvio/caminho alternativo, aguardar, outra estrat�gia).
- [x] Quando chega a um novo aeroporto, deve comunicar a sua chegada ao controlador a�reo
- [ ] Better Var management and mutexes

## communication
- [x] get Destination
- [x] Update Location
- [x] Detect HeartBeat
- [ ] shared memory thread doesnt stop because waiting for writes


## ideas

Problema da posi��o:
	   0 1
	0 |o| |
	1 | |o| 
 
	0,0 quer mover-se para o 1,1
	1,1 quer mover-se para o 0,0

	possibilidades:
	0,0 ->	0,1 ->  1,1
			1,1 ->	0,0

	0,0 ->	1,0 ->  1,1
			1,1 ->	0,0

	1,1 ->	0,1 ->  0,0
			0,0 ->	1,1

	1,1 ->	1,0 ->  1,1
			0,0 ->	1,1



thread do aviao � controlada por um evento no main()
eg: obter coords ao inicio, aciona-se o evento e a thread envia quando terminar acabar o evento.

aviao gera um mapa com nome "mapa-" + PId
control guarda esse mapa no dados->Aviao
usa isso para responder e envia um evento?