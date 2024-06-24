#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <map>
#include <vector>
#include <queue>

using namespace std;
namespace fs = std::filesystem;

class Page;
class Frame;
class BufferManager;
BufferManager* bm;
vector<Page*> paginas;

void menuBUFFER();


// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
//                        PAGE   
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
class Page {
public:
    int id;
    Page(int id) : id(id) {}
    void save() {} 
};

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
//                        FRAME   
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
class Frame {
    private:
        int id;
        Page* pagina;
        bool dirtyFlag;
        int pinCount;
        bool pin; //Pin o unnpin page
        int lastUsed;
        bool refBit;
    
    public:
        queue<bool> dirtyQueue;

        Frame(int id): dirtyFlag(0), pinCount(0), lastUsed(0), refBit(false), pin(false) {
            this->id = id;
            pagina = nullptr;
        }
        
        void resetFrame() {
            delete pagina;
            pagina = nullptr;
            dirtyFlag = false;
            pinCount = 0;
            pin = 0; //Pin o unpin page
            lastUsed = 0;
            refBit = false;
        }

        Page* getPagina() { return pagina; }
        int getId(){ return id;  }
        void setPage(Page *pag) { this->pagina = pag; }

        int getPinCount(){  return pinCount;    }

        bool getRefBit(){  return refBit;    }
        void setRefBit(bool state){  this->refBit = state;      }

        void incrementPinCount() { pinCount++; }
        void decrementPinCount() { if (pinCount > 0) pinCount--; }

        void saveChanges() { if (pagina) pagina->save(); }

        int getLastUsed(){  return lastUsed;    }
        void setLastUsed(int lused) { this->lastUsed = lused; }
        void incrementLastUsed() {   lastUsed++; }

        bool getPin(){  return pin;    }  //Pin o unpin page
        void pinPage() { this->pin = true; }  //Pin o unpin page
        void unpinPage() { this->pin = false; } //Pin o unpin page

        void insertDirtyQueue() {   dirtyQueue.push(dirtyFlag);     }
        void removeDirtyQueue() { dirtyQueue.pop(); }
        void setDirty(bool accion) { dirtyFlag = accion; }
        bool getDirty() { return dirtyQueue.front(); }
};

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
//                        BUFFER MANAGER   
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
class BufferManager {
    private:
        map<int, int> pageTable;
        vector<Frame*> bufferPool;
        string methodReplace;
        int clockHand;
        int miss;
        int hits;
        int solicitudes;

    public:

        /*
        Constructor por defecto
            poolSize = indica el numero de frames   
        */
        BufferManager() : miss(0), hits(0), solicitudes(0), clockHand(0) {
            int poolSize = 4; 
            bufferPool.resize(poolSize);
            for (int i = 0; i < poolSize; i++) {
                bufferPool[i] = new Frame(i);
            }

        }

        /*
        Constructor con parametros
        */
        BufferManager(int poolSize) : miss(0), hits(0), solicitudes(0) {
            bufferPool.resize(poolSize);
            for (int i = 0; i < poolSize; i++) {
                bufferPool[i] = new Frame(i);
            }

            //MRU, LRU, CLOCK
            methodReplace = "MRU"; //INDICADOR DE MÉTODO DE REEMPLAZO
        }

        ~BufferManager() {
            for (auto& frame : bufferPool) {
                delete frame;
            }
        }

        bool isBufferFull() {
            for (auto& frame : bufferPool) {
                if (frame->getPagina() == nullptr) {
                    return false;
                }
            }
            return true;
        }
        
        void printPageTable(){
            cout << "Page table: "<<endl;
            cout << "page_id  frame_id"<< endl;
            for (const auto &entry: pageTable) {
                cout << "{" << entry.first << ", " << entry.second << "}" << endl;
            }
        }

        void aumentarLastUsed(){
            for (auto& frame: bufferPool){
                frame->incrementLastUsed();
            }
        }

        void printFrame(){
            cout << "\t> Buffer Pool\n";
            cout << "\tframe_id    page_id    dirtyBit    pinCount      pinned    ";
            
            if (methodReplace == "MRU" || methodReplace == "LRU ")
                cout << "last_used" << endl;
            else 
                cout << "  refBit" << endl;

            for (auto& frame: bufferPool){
                cout << '\t' << frame->getId() << "\t\t";
                if (frame->getPagina() == nullptr){
                    cout << "-" << "\t  -" << "\t\t-" << "\t    -" << "\t\t-" << endl;

                } else {
                    cout << frame->getPagina()->id << "\t  " <<static_cast<int>(frame->getDirty()) << "\t\t" << frame->getPinCount() << "\t     " << static_cast<int>(frame->getPin());
                    
                    if (methodReplace == "MRU" || methodReplace == "LRU ")
                        cout << "\t\t" << frame->getLastUsed();
                    else 
                        cout << "\t\t" << static_cast<int>(frame->getRefBit());

                    cout << endl;
                }
            }

            if (methodReplace == "CLOCK")
                cout << "\n\t> clockHand -> " << clockHand << endl;
        }

        float hitRate() { return (float)hits/solicitudes; }

        void printQueueReq(Frame* framePage) {
            
            if (framePage == nullptr) { cout << "framePage es null.\n"; return;  }

            // Asegúrate de que dirtyQueue no está vacía
            if (framePage->dirtyQueue.empty()) { cout << "dirtyQueue está vacía.\n"; return;}

            // Imprime los elementos de dirtyQueue
            queue<bool> temp = framePage->dirtyQueue; // crea una copia de dirtyQueue
            while (!temp.empty()) {
                cout << static_cast<int>(temp.front()) << ",";
                temp.pop();
            }
        }

        void pinPage(int pageId, Page* page, bool accion){
            Frame* framePage = nullptr;

            if (pageTable.count(pageId)) {
                hits++;
                framePage = bufferPool[pageTable[pageId]];
                if (framePage->getDirty() && !accion) flushPage(pageId);
                framePage->setDirty(accion);    //si va a cambiar a modo lectura
                framePage->insertDirtyQueue();
                if (methodReplace != "CLOCK") {
                    framePage->setLastUsed(0);  //resetea lastUsed a 0 si se est agregando de nuevo esa pagina
                }      
            }
            else {
                newPage(pageId, page, accion);
                miss++;
                // Solo establece framePage si newPage fue exitoso.
                if (pageTable.count(pageId)) {
                    framePage = bufferPool[pageTable[pageId]];
                }
            }

            // Asegura que framePage no sea nulo antes de intentar usarlo.
            if (framePage) {
                framePage->incrementPinCount(); // Incrementa pinCount porque la página comenzará a ser usada.
                if (methodReplace == "CLOCK") {
                    if (framePage->getPin() == false) //Se verifica que el pin sea falso para poder cambiar el refBit en CLOCK
                        framePage->setRefBit(true); // Establece el bit de referencia a 1 para CLOCK       
                } else {
                    aumentarLastUsed();         // Aumenta lastUsed para todos los frames en uso.
                }
            } else {
                cout << "\tError: No se pudo encontrar o crear un frame para la pagin " << pageId << endl;
                return;
            }         
            
            solicitudes++;
            cout << "\t> INFORMACION DE SOLICITUD\n";
            cout << "\tNro de solicitud: "<< solicitudes << endl;
            cout << "\tMisses: " << miss << endl;
            cout << "\tHits: " << hits << endl;
            cout << "\tCola de requerimientos: ";
            printQueueReq(framePage);
            cout << endl;
        }

        void unpinPage(int pageId) {
            Frame* framePage;
            if (pageTable.count(pageId)) {
                framePage = bufferPool[pageTable[pageId]];

                if (framePage->getDirty() == true) flushPage(pageId);
                framePage->decrementPinCount();
                framePage->setDirty(false); 
                // tenemos que ver la cola de solicitudes, vincular
                framePage->removeDirtyQueue();
                cout << "\tCola de requerimientos: ";
                printQueueReq(framePage);
                cout << endl;
            }
        }
        
        void newPage(int pageId, Page* page, bool accion){
            Frame* framePage = nullptr;
            
            bool bufferIsFull = isBufferFull(); // Verifica si el buffer está lleno

            int selectedLastUsed;
            
            // Verifica si el buffer está lleno y ajusta la búsqueda de frame libre
            for (auto& frame : bufferPool) {
                if (frame->getPin() == false && frame->getPinCount() == 0) { // Se añade la verificacion de pin (si está fijado o no)
                    if (!bufferIsFull) {
                        framePage = frame;
                        break;
                    } else {
                        // Si el buffer está lleno, aplica las políticas MRU y LRU correctamente
                        if ((methodReplace == "MRU" && frame->getLastUsed() < selectedLastUsed) || 
                            (methodReplace == "LRU" && frame->getLastUsed() > selectedLastUsed)) {
                            framePage = frame;
                            selectedLastUsed = frame->getLastUsed();  // Actualiza el valor de comparación
                        }
                    }
                }
            }

            // Si después del ciclo no se encontró ningún frame, maneja según la política
            if (!framePage) {
                framePage = replaceFrame();

                // Manejo de errores si aún no hay frame disponible
                if (!framePage) {
                    cout << "\tNo se pudo agregar la pagina porque no hay frames libres ni reemplazables." << endl;
                    return;                 // Salir si no se encuentra un frame reemplazable
                }
            }

            // Configura el frame con la nueva página
            framePage->setPage(page); 
            pageTable[pageId] = framePage->getId();
            framePage->setDirty(accion);    // Asigna si está en escritura o en lectura
            framePage->insertDirtyQueue();  //Inserta el requerimiento en la cola
            if (methodReplace == "CLOCK") {
                framePage->setRefBit(true); // Establece el bit de referencia a 1 para CLOCK       
            } else {
                framePage->setLastUsed(0);  // Resetea lastUsed a 0 para la nueva página
            }

        }

        Frame* replaceFrame() {
            Frame* framePage = nullptr;
            if (methodReplace == "MRU") {
                framePage = replaceFrameMRU();
            } else if (methodReplace == "LRU") {
                framePage = replaceFrameLRU();
            } else if (methodReplace == "CLOCK") {
                framePage = replaceFrameCLOCK();
            } else {
                return nullptr;
            }

            return framePage;
        }

        Frame* replaceFrameLRU() {
            Frame* frameToReplace = nullptr;
            int highestLastUsed = -1;

            for (auto& frame : bufferPool) {
                cout << "Frame ID: " << frame->getId() << " Pin Count: " << frame->getPinCount() << " Last Used: " << frame->getLastUsed() << endl;
                if (frame->getPin() == false && frame->getPinCount() == 0 && frame->getLastUsed() > highestLastUsed ) { // Se añade la verificacion de pin (si está fijado o no)
                    frameToReplace = frame;
                    highestLastUsed = frame->getLastUsed();
                }
            }

            if (!frameToReplace) {
                cout << "\t\nNo hay frames disponibles para reemplazar. Todos están en uso." << endl;
                return nullptr; 
            }

            // Procede a reemplazar el frame seleccionado.
            if (frameToReplace->getDirty()) {
                flushPage(frameToReplace->getPagina()->id); // Guardar cambios si está dirty
            }   
            freePage(frameToReplace->getPagina()->id);      // Liberar la página actual del frame

            return frameToReplace;
        }

        Frame* replaceFrameMRU() {
            Frame* frameToReplace = nullptr;
            int lowestLastUsed = 2147483647; 

            for (auto& frame : bufferPool) {
                cout << "---------- Frame ID: " << frame->getId() << " Pin Count: " << frame->getPinCount() << " Last Used: " << frame->getLastUsed() << endl;
                if (frame->getPin() == false && frame->getPinCount() == 0 && frame->getLastUsed() < lowestLastUsed) {  // Se añade la verificacion de pin (si está fijado o no)
                    frameToReplace = frame;
                    lowestLastUsed = frame->getLastUsed();
                }
            }

            if (!frameToReplace) {
                cout << "\t\nNo hay frames disponibles para reemplazar. Todos están en uso." << endl;
                return nullptr;
            }

            // Procede a reemplazar el frame seleccionado.
            if (frameToReplace->getDirty()) {
                flushPage(frameToReplace->getPagina()->id); // Guardar cambios si está dirty
            }
            freePage(frameToReplace->getPagina()->id);      // Liberar la página actual del frame

            return frameToReplace;
        }

        Frame* replaceFrameCLOCK() {
            int bufferPoolSize = bufferPool.size();
            int counter = 0;
            
            cout << "\n\t > SIMULANDO CLOCK" << endl;
            cout << "\t clockHand -> " << clockHand << endl;

            while (true) {
                Frame* frame = bufferPool[clockHand];

                if (frame->getPin() == false) {  // Se añade la verificacion de pin (si está fijado o no)
                    if (frame->getRefBit()) {
                        frame->setRefBit(false);
                        cout << "\t*** SECOND CHANCE! ***" << endl;
                    } else {
                        // Procede a reemplazar el frame seleccionado.
                        if (frame->getPinCount() == 0) {

                            if (frame->getDirty()) {
                                cout << endl;
                                flushPage(frame->getPagina()->id); // Guardar cambios si está dirty
                                cout << endl;
                            }
                            freePage(frame->getPagina()->id); // Liberar la página actual del frame
                            clockHand = (clockHand + 1) % bufferPoolSize;
                            cout << "\t clockHand -> " << clockHand << endl;
                            
                            return frame;
                        } 
                    }
                }

                clockHand = (clockHand + 1) % bufferPoolSize;
                counter++;
                cout << "\t clockHand -> " << clockHand << endl;

                // Si hemos recorrido todo el bufferPool y no hemos encontrado un frame para reemplazar
                if (counter == bufferPoolSize) {
                    cout << "\t\nNo hay frames disponibles para reemplazar. Todos están en uso." << endl;
                    return nullptr; 
                }
            }
        }

        void freePage(int pageId){
            Frame* framePage;
            // Resetea el frame para usar otra vez
            if (pageTable.count(pageId)) {
                framePage = bufferPool[pageTable[pageId]];
                framePage->resetFrame();
            }
            // Eliminar la entrada de la página vieja en pageTable
            pageTable.erase(pageId);
        }

        void flushPage(int pageId) {
            Frame* framePage;
            if (pageTable.count(pageId)) {
                framePage = bufferPool[pageTable[pageId]];
                
                cout << "\tGuardar cambios de la pagina " << framePage->getPagina()->id << "? (true = 1 / false = 0)?: ";
                bool saveC;
                cin >> saveC;
                if (saveC){
                    framePage->saveChanges();
                    cout << "\tSe guardaron los datos con exito!\n";
                } else{
                    cout << "\tNo se guardaron los cambios\n";
                }
                
            }
        }

        bool pageExists(int pageId) {
            // Usa la pageTable para verificar si el ID de página está presente
            return pageTable.count(pageId) > 0;
        }

        void markPagePinned(int pageId) {
            Frame* framePage;
            if (pageTable.count(pageId)) {
                framePage = bufferPool[pageTable[pageId]];
                if (framePage->getPin()) {
                    cout << "\tERROR: La pagina " << pageId << " ya esta fijada!\n";
                } else {
                    framePage->pinPage();
                    cout << "\tPagina " << pageId << " fijada.\n";
                }
            }
        }

        void markPageUnpinned(int pageId) {
            Frame* framePage;
            if (pageTable.count(pageId)) {
                framePage = bufferPool[pageTable[pageId]];
                if (!framePage->getPin())
                    cout << "\tERROR: La pagina " << pageId << " no esta fijada!\n";
                else {
                    framePage->unpinPage();
                    cout << "\tPagina " << pageId << " desfijada.\n";
                }
            }
        }
};




// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
//                        BLOQUE   
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
class Bloque {
private:
    vector<fs::path> sectores;
    fs::path bloqueFilePath;
    int capacidadBloque;
    int espacioUsado = 0;
    int sectorCount = 0; 

public:
    Bloque(const fs::path& path, int capacidadInicial) : bloqueFilePath(path), capacidadBloque(capacidadInicial) {
        if (!fs::exists(bloqueFilePath)) {
            ofstream outfile(bloqueFilePath);
            if (!outfile) {
                throw std::runtime_error("No se pudo crear el archivo del bloque.");
            }
            outfile.close();
        }
    }

    fs::path getFilePath() const {
        return bloqueFilePath;
    }

    bool agregarDatos(const std::string& datos) {
        if (sectorCount >= 2) {  // Verifica si el bloque ya tiene dos sectores llenos
            return false;  // No hay espacio para más sectores en este bloque
        }

        std::ofstream outfile(bloqueFilePath, std::ios::app);
        if (!outfile) {
            throw std::runtime_error("No se pudo abrir el archivo del bloque para escribir.");
        }

        //cout << datos << endl;
        outfile << datos;
        outfile.close();
        sectorCount++;  // Incrementamos el contador de sectores llenados en el bloque
        return true;
    }



    int getEspacioDisponible() const {
        return capacidadBloque - espacioUsado;
    }
/*
    void agregarSector(const fs::path& rutaSector) {
        if (sectores.size() < capacidadBloque) {
            sectores.push_back(rutaSector);
            actualizarArchivo();
        } else {
            throw std::runtime_error("Capacidad del bloque excedida.");
        }
    }

    void actualizarArchivo() {
        ofstream outfile(bloqueFilePath, ios::out);
        if (!outfile) {
            throw std::runtime_error("No se pudo abrir el archivo del bloque para actualizar.");
        }
        for (const fs::path& sector : sectores) {
            outfile << sector << "\n";
        }
        outfile.close();
    }

    void escribirContenidoSector(const fs::path& sectorPath) {
        ifstream sectorFile(sectorPath);
        ofstream outfile(bloqueFilePath, ios::app); 

        if (!sectorFile || !outfile) {
            throw runtime_error("Error al abrir archivos para operación de sector.");
        }

        string content;
        while (getline(sectorFile, content)) {
            cout << "agregar aa: " << content << endl;
            outfile << content << "\n"; 
        }
        outfile.close();
    }

    void limpiarBloque() {
        sectores.clear();  // Limpiar la lista de sectores

        // Abrir el archivo del bloque y vaciar su contenido
        ofstream outfile(bloqueFilePath, ios::trunc);  // El modo 'trunc' truncará el archivo a cero longitud
        if (!outfile) {
            throw runtime_error("No se pudo abrir el archivo del bloque para limpiar.");
        }
        outfile.close();  // Cerrar el archivo después de truncar
    }
*/

    int getCapacidadBloque() const { return capacidadBloque; }
    void setCapacidadBloque(int nuevaCapacidad) { capacidadBloque = nuevaCapacidad; }

    void ocuparEspacio(int tamSector) {
        if (tamSector <= capacidadBloque) {
            capacidadBloque -= tamSector;
        } else {
            throw std::runtime_error("No hay suficiente espacio en el bloque para ocupar.");
        }
    }
};

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
//                        DISCO
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
class Disco {
private:
    fs::path rootPath;
    int numPlates, numSurfaces, numTracks, numSectors;
    int total_capacity;
    int used_capacity;

    int sector_capacity;

public:
    // Constructor con parametros
    Disco(const fs::path& rootPath, int _numPlates, int _numTracks, int _numSectors, int _sector_capacity)
        : rootPath(rootPath){
        this -> numPlates = _numPlates;
        this -> numTracks = _numTracks;
        this -> numSectors = _numSectors;
        this -> sector_capacity = _sector_capacity;

        this -> total_capacity = _numPlates * 2 * _numTracks * _numSectors * _sector_capacity;
        this -> used_capacity = 0;
        createStructure();
    }

    // Constructor por defecto
    Disco(const fs::path& rootPath)
        : rootPath(rootPath){
        this -> numPlates = 2;
        this -> numTracks = 5;
        this -> numSectors = 3;
        this -> sector_capacity = 1000;

        this -> total_capacity = 2 * 2 * 5 * 3 * 1000;
        this -> used_capacity = 0;
        createStructure();
    }

    // Método para crear la estructura de directorios
    void createStructure() {
        for (int p = 0; p < numPlates; ++p) {
            for (int s = 0; s < 2; ++s) {
                for (int t = 0; t < numTracks; ++t) {
                    fs::path trackPath = rootPath / ("Plato" + to_string(p)) / ("Superficie" + to_string(s)) / ("Pista" + to_string(t));
                    fs::create_directories(trackPath);
                    for (int sec = 0; sec < numSectors; ++sec) {
                        fs::path filePath = trackPath / ("Sector" + to_string(sec) + ".txt");
                        ofstream outFile(filePath);
                        outFile.close(); 
                    }
                }
            }
        }
    }

    // Método para obtener la ruta de un sector específico
    fs::path getSectorPath(int plate, int surface, int track, int sector) {
        return rootPath / ("Plato" + to_string(plate)) / ("Superficie" + to_string(surface)) / ("Pista" + to_string(track)) / ("Sector" + to_string(sector) + ".txt");
    }

    fs::path getRootPath() const {
        return rootPath;
    }

    // Método para contar los registros en un sector
    int countRecordsInSector(const fs::path& sectorPath) {
        ifstream sectorFile(sectorPath);
        string line;
        int count = 0;

        if (!sectorFile) {
            cerr << "No se pudo abrir el archivo de sector: " << sectorPath << endl;
            return 0;
        }

        while (getline(sectorFile, line)) {
            if (!line.empty()) {
                count++;
            }
        }
        sectorFile.close();
        return count;
    }

    // Método para calcular el tamaño ocupado de un sector
    int calculateSectorSize(fs::path sectorPath, int recordSize) {
        int numRecords = countRecordsInSector(sectorPath);
        return numRecords * recordSize;
    }

    int getnumPlates(){  return numPlates;    }
    int getnumSurfaces(){  return numSurfaces;    }
    int getnumTracks(){  return numTracks;    }
    int getnumSectors(){  return numSectors;    }
    int getSectorCapacity(){  return sector_capacity;    }
    int espacioLibre() {    return total_capacity - used_capacity;  }
    int getCapacidadTotal() {   return total_capacity;  }
    int getCapacidadUtilizada() {   return used_capacity;   }
};

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
//                        DISK MANAGER
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------

class DiskManager {
private:
    Disco *disco;
    vector<Bloque> bloques;
    int cant_bloques = 5;   // AQUI SE DETERMINA LA CANTIDAD DE BLOQUES

public:
    // Constructor
    DiskManager(const string& path) {
        disco = new Disco(path);
        inicializarBloques();
    }

    // Destructor para evitar fugas de memoria
    ~DiskManager() {
        delete disco;
    }

    void inicializarBloques() {
        fs::path rootBlocks = "bloques";
        for (int i = 0; i < cant_bloques; i++) {
            fs::path bloquePath = rootBlocks /("bloque" + to_string(i) + ".txt");
            Bloque nuevoBloque(bloquePath, getTamBloques());
            bloques.push_back(nuevoBloque);
        }
    }

    void rellenarBloques() {
    int numSectores = disco->getnumSectors();
        string dataBuffer;

        for (int i = 0; i < numSectores; i++) {
            string data = readData(0, 0, 0, i);
            if (data.empty()) continue;

            dataBuffer += data;  // Acumulamos datos de dos sectores

            // Intentar añadir al bloque cada dos sectores o al final del último sector
            if ((i + 1) % 2 == 0 || i == numSectores - 1) {
                bool dataAdded = false;
                for (Bloque& bloque : bloques) {
                    if (bloque.agregarDatos(dataBuffer)) {
                        dataAdded = true;
                        dataBuffer.clear();  // Limpiamos el buffer después de su uso
                        break;
                    }
                }

                if (!dataAdded) {
                    std::cerr << "No hay suficiente espacio en los bloques para los datos acumulados de los sectores." << std::endl;
                }
            }
        }
    }

    // Método para obtener el numero de bloques
    int getNumBloques() const {
        return bloques.size();
    }

    // Método para obtener el tamaño de los bloques
    int getTamBloques() const {
        return disco->getCapacidadTotal() / cant_bloques;
    }

    // Método para escribir datos en un sector específico
    bool writeData(const string& data, int plate, int surface, int track, int sector) {
        fs::path filePath = disco->getSectorPath(plate, surface, track, sector);
        ofstream outFile(filePath);
        if (outFile.is_open()) {
            outFile << data;
            outFile.close();
            return true;
        }
        return false;
    }

    string readData(int plate, int surface, int track, int sector) {
        fs::path sectorPath = disco->getSectorPath(plate, surface, track, sector);
        ifstream inFile(sectorPath);
        stringstream buffer;
        string line;
        while (getline(inFile, line)) {
            buffer << line << "\n";  // Suponemos que un sector puede tener múltiples líneas
        }
        return buffer.str();
    }

    string obtenerTipoDato(const string& nombreColumna) {
        string tipo_dato;
        bool tipo_valido = false;
        do {
            cout << "\t-"<<nombreColumna << ": ";
            cin >> tipo_dato;
            tipo_valido = tipo_dato == "int" || tipo_dato == "string" || tipo_dato == "float" || tipo_dato == "char";
            if (!tipo_valido) {
                cout << "Tipo de dato ingresado incorrectamente. Elija entre int, string, float, o char.\n";
            }
        } while (!tipo_valido);
        return tipo_dato;
    }

    // Método para crear el esquema (informacion de la tabla)
    void createSchema(const string& line) {
        string schemaFileName = "esquema.txt";
        ofstream esquema(schemaFileName); 

        if (!esquema) {
            cerr << "No se puede abrir el archivo de esquema para escritura." << endl;
            return;
        }

        // Primero agregamos el nombre de la tabla
        cout << "\tNombre de la relación: ";
        string nameTable;
        cin >> nameTable;
        esquema << nameTable << "#";

        istringstream iss(line);
        string columnName, tipo_dato, tam;
        int totalRecordSize = 0;  // Almacenar el tamaño total del registro de longitud fija

        // Luego agregamos el nombre de las columnas y sus tipos
        while (getline(iss, columnName, ',')) {
            tipo_dato = obtenerTipoDato(columnName);

            if (tipo_dato == "int" || tipo_dato == "float") {
                tam = "16";  // Suponiendo 16 bytes para int y float
                totalRecordSize += 16;
            } else if (tipo_dato == "string") {
                cout << "Tamaño de la columna: ";
                cin >> tam;  // Permitimos al usuario especificar el tamaño
                int tamString = stoi(tam);
                totalRecordSize += tamString;
            } else if (tipo_dato == "char") {
                tam = "1";  // Suponiendo 1 byte para char
                totalRecordSize += 1;
            }

            esquema << columnName << "#" << tipo_dato << "#" << tam << "#"; 
        }
        esquema << to_string(totalRecordSize);
        esquema.close();
    }

    // metodo para modificar el tamaño de la palabra
    string modificarTamPalabra(const string& palabra, int posicionPalabra) {
        ifstream archivoEsquema;
        archivoEsquema.open("esquema.txt");

        string lineaEsquema, palabraEsquema, tipoEsquema, tamColumnEsquema;
        getline(archivoEsquema, lineaEsquema);
        stringstream sss(lineaEsquema);

        getline(sss, palabraEsquema, '#'); // nombre de la relacion
        for (int i=0; i<posicionPalabra; i++){
            getline(sss, palabraEsquema, '#');
            getline(sss, tipoEsquema, '#');
            getline(sss, tamColumnEsquema, '#');
        }

        archivoEsquema.close();

        int tam = stoi(tamColumnEsquema);
        string palabraTMP = palabra;
        palabraTMP.resize(tam, ' ');  // Rellena o trunca la palabra para que tenga el tamaño correcto

        return palabraTMP;
    }
    
    // Método para convertir un registro a longitud fija
    string transformRLF(const string& line) {
        ifstream archivoNombreColumnas("esquema.txt");
        string lineaEsquema, palabraEsquema,tipoEsquema;
        getline(archivoNombreColumnas, lineaEsquema);
        stringstream sss(lineaEsquema);
        
        bool en_comillas = false;
        string registro; 
        string palabra;
        int posicionPalabra = 0;

        for(char c : line){
            getline(sss, palabraEsquema, '#'); 
            
            if (c == '"'){ 
                en_comillas = !en_comillas;
                continue;
            }
            if (c == ',' && !en_comillas){ 
                posicionPalabra++;
                palabra = modificarTamPalabra(palabra, posicionPalabra);
                registro = registro + palabra;
                palabra.clear();
            }
            else {
                palabra = palabra + c;
            }
        }
        registro = registro + palabra;
        archivoNombreColumnas.close();

        return registro;
    }

    // Método para obtener el tamaño del registro en base al esquema
    int getRecordSizeFromSchema() {
        ifstream schemaFile("esquema.txt");
        string line;
        string lastValue;
        int recordSize = 0;

        while (getline(schemaFile, line)) {
        }

        size_t pos = line.rfind('#');
        if (pos != string::npos) {
            lastValue = line.substr(pos + 1);
            recordSize = stoi(lastValue); 
        }
        //cout << recordSize << endl;
        return recordSize;
    }

    // Método para escribir datos desde un archivo CSV en sectores
    void writeDataFromCSV(const string& csvPath) {
        ifstream csvFile(csvPath);
        string line;
        int lineCount = 0;
        int plate = 0, surface = 0, track = 0, sector = 0;
        
        stringstream buffer;

        // Crear el esquema a partir de la primera línea
        getline(csvFile, line);
        createSchema(line);

        int recordSize = getRecordSizeFromSchema();         // Tamaño de registros
        int sectorCapacity = disco->getSectorCapacity();    // Capacidad de sector
        int recordsPerSector = sectorCapacity / recordSize;

        while (getline(csvFile, line)) {
            string newLine = transformRLF(line);
            buffer << newLine << "\n";
            lineCount++;
            
            if (lineCount == recordsPerSector) {
                writeData(buffer.str(), plate, surface, track, sector);
                buffer.str(""); 
                buffer.clear();
                lineCount = 0; 
                sector++; 

                if (sector >= disco->getnumSectors()) {
                    sector = 0;
                    track++;
                    if (track >= disco->getnumTracks()) {
                        track = 0;
                        surface++;
                        if (surface >= disco->getnumSurfaces()) {
                            surface = 0;
                            plate++;
                            if (plate >= disco->getnumPlates()) {
                                std::cout << "Exceeded disk capacity.\n";
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (!buffer.str().empty()) {
            writeData(buffer.str(), plate, surface, track, sector);
        }
    }

    // Metodo que devuelve el tamañon del disco
    int capacidadTotalDisco() {
        return disco->getCapacidadTotal();
    }

    int capacidadUtilizadaDisco() {
        return disco->getCapacidadUtilizada();
    }

    int espacioLibreDisco() {
        return disco->espacioLibre();
    }

};


//*********************************************************************************************************************************************************************
DiskManager dm("pathDisco");


/*

------------------- MENUS -------------------------------------

*/

void printBlockX(){
    cout << "Nro de bloque:";
}

void menuDISCO() {
    char opcion;
    cout << endl << endl;
    cout << "---------------------------\n";
    cout << ">>>> MEGATRON\n";
    cout << "---------------------------\n";

    do {
        cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><\n";
        cout << ">> OPTIONS\n";
        cout << "1. Mostrar capacidad total del disco\n";
        cout << "2. Mostrar capacidad utilizada\n";
        cout << "3. Mostrar capacidad libre\n";
        cout << "4. Mostrar cantidad de bloques\n";
        cout << "5. Mostrar la capacidad total del bloque \n";

        //cout << "6. Mostrar contenido del bloque\n";
        cout << "7. Mostrar Menu Buffer\n";        
        cout << "8. Salir\n";
        cout << "Opcion? ";
        cin >> opcion;

        switch(opcion) {
            case '1':
                cout << "\tCapacidad total del disco: " << dm.capacidadTotalDisco() << endl;
                break;
            case '2': 
                cout << "\tCapacidad utilizada del disco: " << dm.capacidadUtilizadaDisco() << endl;
                break;
            case '3':
                cout << "\tCapacidad libre del disco: " << dm.espacioLibreDisco() << endl;
                break;
            case '4':
                cout << "\tCantidad de bloques: " << dm.getNumBloques() << endl;
                break;
            case '5':
                cout << "\tCapacidad total del bloque: " << dm.getTamBloques() << endl;
                break;
            /*
            case '6':
                printBlockX();
                break;
            */
            case '7':
                menuBUFFER();
                opcion = 8;
                break;
            case '8':
                cout << "\t> ADIOS!\n";
                break;
            default:
                cout << "Opción inválida. Por favor, intente de nuevo.\n";
                break;
        }
    } while (opcion != '8');
}

void solicitarPagina(){
    cout << "\t> SOLICITUD\n";
    int idPagina;
    char accion;
    do{
        cout << "\tID de la pagina: ";
        cin >> idPagina;
    } while (idPagina > 100 || idPagina <= 0);
    
    cout << "\tLectura (R) o escritura (W)?: ";
    cin >> accion;
    if (accion == 'R'){
        accion = false;
    } else if (accion == 'W'){
        accion = true;
    }
    cout << endl;
    bm->pinPage(idPagina, paginas[idPagina-1], accion);
}

void liberarPagina() {
    int pageId;
    cout << "\tIngrese el ID de la pagina a liberar: ";
    cin >> pageId;

    if (bm->pageExists(pageId)) { 
        bm->unpinPage(pageId);
        cout << "\tPinCount de la pagina ha sido decrementado.\n";
    } else {
        cout << "\tLa pagin con ID " << pageId << " no existe en el buffer.\n";
    }
}

void menuBUFFER()
{
    char opcion;
    int pageId;
    cout << "---------------------------\n";
    cout << ">>>> BUFFER MANAGER\n";
    cout << "---------------------------\n";

    cout << "Numero de frames: ";
    int numFrames;
    cin >> numFrames;
    bm = new BufferManager(numFrames);

    do {
        cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><\n";
        cout << ">> OPTIONS\n";
        cout << "1. Solicitar una pagina\n";
        cout << "2. Fijar una pagina\n";
        cout << "3. Desfijar una pagina\n";
        cout << "4. Liberar una pagina\n";        
        cout << "5. Imprimir Page Table (Buffer Pool)\n";
        cout << "6. Imprimir Hit Rate\n";
        cout << "7. Salir\n";
        cout << "Opcion? ";
        cin >> opcion;
        
        switch(opcion)
        {
            case '1':
                solicitarPagina();
                break;
            case '2': 
            {
                int pageId;
                cout << "\tIngrese el ID de la pagina a fijar: ";
                cin >> pageId;

                if (bm->pageExists(pageId))
                    bm->markPagePinned(pageId);
                else
                    cout << "\tLa pagina con ID " << pageId << " no existe en el buffer.\n";
                break;
            }
            case '3':
            {
                int pageId;
                cout << "\tIngrese el ID de la pagina a desfijar: ";
                cin >> pageId;

                if (bm->pageExists(pageId))
                    bm->markPageUnpinned(pageId);
                else
                    cout << "\tLa pagina con ID " << pageId << " no existe en el buffer.\n";
                break;
            }
            case '4':
                liberarPagina();
                break;
            case '5':
                bm->printFrame();
                break;
            case '6':
                cout << "\t> HIT RATE: " << bm->hitRate() << endl;
                break;
            case '7':
                cout << "\t> ADIOS!\n";
                break;
            default:
                break;
        }
    } while (opcion != '7');
}

int main (){
    cout << "Se cargaran los datos del csv............" << endl;
    dm.writeDataFromCSV("titanic.csv");
    dm.rellenarBloques();
    
    paginas.resize(100);
    for (int i = 0; i < 100; i++) {
        paginas[i] = new Page(i+1);
    }

    menuDISCO();

    return 0;
}