# Normalizzatore

**Normalizzatore** è un'applicazione multipiattaforma per la normalizzazione di dati, sviluppata in C++ con interfaccia Qt. Il progetto è organizzato in branch separati per Linux e Windows, con struttura modulare e supporto per librerie di terze parti.

---

## Struttura del progetto

| Sistema | Cartelle principali | Note <br>
|**Linux**   | `Normalizzatore/`  | Direcory principale nel brach `Linux` <br>
|            | `README.MD`        |<br>
|            | `thirdparty/`      |<br>
|            | `src/`             |<br>
|            | `CMakeLists.txt`   |<br>

---

## Branch
- `master`: contiene README e cartella `thirdparty`
- `Linux`: contiene il codice per le cartelle specifiche di Linux
- `Windows`: contiene il codice per le cartelle specifiche di Windows

---

## Requisiti
- C++ 17 o superiore
- Qt5 o superiore
- CMake 3.10 o superiore
- Compilatore compatibile con il sistema operativo: GCC/Clang su Linux, MSVC su Windows

---

## Applicazione
- Disponibile `*.AppImage` per Linux (Ubuntu 24.04 LTS o superiore / Fedora 43 o superiore)
- Disponibile file `*.exe` per Windows (10 o superiore)

---

## Compilazione

### Linux
`cd Normalizzatore`
`mkdir build`
`cd build`
`cmake ..`
`make -j$(nproc)`
`./Normalizzatore`

### Windows
- Aprire `CMakeLists.txt` con Visual Studio
- Generare la soluzioneù
- Compilare e avviare il progetto

---

## Uso
L'interfaccia principale fornisce:
- **Pulsante normalizza**: avvia la normalizzazione dei dati
- **LOG**: visualizza i messaggi di processo
- **Status**: mostra lo stato corrente dell'operazione

---

## Contributi
Le modifiche devono essere inviate tramite branch specifici (Linux/Windows) e merge su `master` solo per file condivisi come README e `thirdparty`