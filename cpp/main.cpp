#include <iostream>
#include "adbconnect.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <windows.h>
#include <chrono>
#include <thread>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Pour lecture image

using namespace std;

const string PARENT_IMG_FOLDER = "./img/"; // /!\ IMPORTANT : ce dossier doit être préalablement créé.
// Dossier dans lequel sera stockée l'image

ADBConnect adb;

struct VirtualSolution {
    vector<bool> finalGrid;

    vector<int> posObj;
    vector<int> orderObj;

    bool passByEmptyGrid; // La solution passe par une grille vide ?
    // Le jeu attribue un bonus si la grille est entèrement vidée (peu importe si tous les objets sont placés).
    //  Le bot va donc choisir la solution ayant le meilleur score, et (si possible) celle qui passe par une grille vide.


    void addObj(const vector<bool>& newFinalGrid, const int& newPosObj) {
        finalGrid = newFinalGrid;
        posObj.push_back(newPosObj);

        passByEmptyGrid = passByEmptyGrid || !(find(newFinalGrid.begin(), newFinalGrid.end(), 1) != newFinalGrid.end());
    }
};

struct PixelColor {
    unsigned char r, g, b;
};

struct ColorToCompare {
    PixelColor color;
    int tolerance;

    bool matchColor(const PixelColor& comparedColor) {
        return abs(color.r - comparedColor.r) < tolerance && abs(color.g - comparedColor.g) < tolerance && abs(color.b - comparedColor.b) < tolerance;
    }
};

struct Coord {
    int x;
    int y;

    bool operator==(const Coord& other) const {
        return x == other.x && y == other.y;
    }
};

struct GridConfig {
    int nbColumns;
    int nbRows;
    double blocSize;

    Coord startPos;
};


// Image
int width, height, channels;


// Grille
const int X_START_GRID = 122;
const int Y_START_GRID = 610;

const int NB_COLUMNS = 8;
const int NB_ROWS = 8;
const int BLOCK_SIZE = 118;

const GridConfig gcmain = {
    NB_COLUMNS,
    NB_ROWS,
    BLOCK_SIZE,
    {X_START_GRID, Y_START_GRID}
};

const ColorToCompare COLOR_EMPTY = {{24, 37, 66}, 15}; // r, g, b, tolerance
//const int TOLERANCE_GRID = 15;
const bool PRINT_GRID = true;


// Grilles des objets
const vector<int> X_START_OBJ_GRID = {158, 471, 786}; // https://fr.pixspy.com/
const int Y_START_OBJ_GRID = 1721;

const int NB_COLUMNS_OBJ = 6;
const int NB_ROWS_OBJ = 6;
const int BLOCK_SIZE_OBJ = 26;

GridConfig gcobj = {
    NB_COLUMNS_OBJ,
    NB_ROWS_OBJ,
    BLOCK_SIZE_OBJ,
    {0, Y_START_OBJ_GRID}
};

const ColorToCompare COLOR_EMPTY_OBJ = {{58, 81, 148}, 25}; // r, g, b, tolerance
const bool PRINT_OBJECTS = true;


struct Obj {
    vector<Coord> coord_format;
    vector<bool> grid_format;
    string spec;

    bool operator==(const Obj& other) const {
        return coord_format == other.coord_format;
    }
};

const unordered_map<string, Obj> specific_obj = {
    {
        "4v",
        {
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}}
        }
    },
    {
        "5v",
        {
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}}
        }
    },
    {
        "4h",
        {
            {{0, 0}, {1, 0}, {2, 0}, {3, 0}}
        }
    },
    {
        "5h",
        {
            {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}
        }
    }
};

Obj hBar3 = { {}, {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0
} };
Obj vBar3 = { {}, {
    0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0
} };

bool isBar3(const Obj& o) {
    return o.grid_format == hBar3.grid_format || o.grid_format == vBar3.grid_format;
}

// Reflexion
const int MAX_ASYNC_TASK = 3;

const int REFLEXION_STEP = 3;
const int UIB_4_MULT = 3; // Multiplicateur pour les cases vides avec 4 voisines vides
const int UIB_3_MULT = 2; // Multiplicateur pour les cases vides avec 3 voisines vides
const int UIB_2_MULT = -1; // Multiplicateur pour les cases vides avec 2 voisines vides
const int UIB_1_MULT = -1.5; // Multiplicateur pour les cases vides avec 1 voisine vide
const int IB_0_MULT = -2; // Multiplicateur pour les cases vides avec 0 voisine vide

const int IFB_MULT = -5; // Multiplicateur pour les cases pleines avec 0 voisines pleines
const int UIFB_1_MULT = -4; // Multiplicateur pour les cases pleines avec seulement 1 voisine pleine

const Obj O3X3 = { {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {0, 2}, {1, 2}, {2, 2}} }; // Objet 3x3
const int O3X3_POINT = 25; // Points attribués si un 3x3 rentre dans la grille
const Obj O1X5 = { {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}} }; // Objet 1x5
const int O1X5_POINT = 15; // Points attribués si un 1x5 rentre dans la grille
const Obj O5X1 = { {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}} }; // Objet 5x1
const int O5X1_POINT = 15; // Points attribués si un 5x1 rentre dans la grille


/* Déplacements /!\ specifique à l'app BlockBlast /!\ */
const int INCREASE_Y = 308;
const double ky = 1.33;
const double kx = 1.3;


// IMPORTANT : NE PAS TOUCHER !!!
bool deviceconfigured = false; // NE PAS TOUCHER
string device = ""; // NE PAS TOUCHER
string nameIMG = ""; // NE PAS TOUCHER
string pathIMG = ""; // NE PAS TOUCHER



void printVector(const vector<Coord>& v) {
    cout << "{";
    for (int i = 0; i < (int)v.size(); i++) {
        cout << "{" << v.at(i).x << ", " << v.at(i).y << "}";
        if (i + 1 < (int)v.size()) cout << ", ";
    }
    cout << "}";
}
void printVector(const vector<int>& v) {
    cout << "{";
    for (int i = 0; i < (int)v.size(); i++) {
        cout << v.at(i);
        if (i + 1 < (int)v.size()) cout << ", ";
    }
    cout << "}";
}
void printVector(const vector<bool>& v) {
    cout << "{" << endl;
    for (int i = 0; i < (int)v.size(); i++) {
        cout << '\t' 
             << (v.at(i) == true ? "1" : "0");
        if (i + 1 < (int)v.size()) cout << ", ";
    }
    cout << endl << "}";
}


PixelColor getPixel(const int x, const int y, const unsigned char* data) {
    int index = (y * width + x) * channels;
    /*
    On cherche l'index du pixel dans ce tableau :
    y * width => ligne
    + x => colonne

    * channels => chaque case du tableau contient une
    seule valeure :
    data est un tableau qui contient :
    1:R ; 2:G ; 3:B ; 4:A ; 5:R, etc...

    1 pixel s'étend donc sur channels cases.
    On multplie donc par channels pour avoir l'emplecement du
    pixel.

    Comme dit ci-dessus, chaque case contient une seule
    valeure (R ou G ou B ou A). On peut donc obtenir le
    r, g, et b du pixel en ajoutant 1 ou 2 à partir de
    l'index trouvé.
    */

    const unsigned char r = data[index + 0]; // unsigned char = nb entre 0 et 255
    const unsigned char g = data[index + 1];
    const unsigned char b = data[index + 2];

    return  {r, g, b};
}

Obj getGridFilling(const unsigned char* data, const GridConfig& grid, ColorToCompare colorEmpty, const bool toPrint) {
    Obj obj;
    for (int i = 0; i < grid.nbColumns * grid.nbRows; i++) {
        const int actCol = i % grid.nbColumns;
        const int actRow = i / grid.nbColumns;

        const double x = grid.startPos.x + grid.blocSize * actCol;
        const double y = grid.startPos.y + grid.blocSize * actRow;

        PixelColor colorBloc = getPixel(x, y, data);

        const bool isEmpty = colorEmpty.matchColor(colorBloc);

        obj.grid_format.push_back(!isEmpty);

        if (toPrint) {
            if (i % grid.nbColumns == 0 && i != 0) {
                cout << endl;
            }

            if (isEmpty) {
                cout << " . ";
            }
            else {
                cout << " X ";
            }
        }
    }

    if (isBar3(obj)) {
        cout << endl << "BAR3\t";
        PixelColor p1 = getPixel( // Le plus haut
            grid.startPos.x + 2.5 * grid.blocSize,
            grid.startPos.y - 2 * grid.blocSize,
            data
        );
        PixelColor p2 = getPixel( // Un peu moins haut
            grid.startPos.x + 2.5 * grid.blocSize,
            grid.startPos.y - grid.blocSize,
            data
        );

        if (colorEmpty.matchColor(p1) && !colorEmpty.matchColor(p2)) {
            obj.spec = "4v";
        }
        else if (!colorEmpty.matchColor(p1) && !colorEmpty.matchColor(p2)) {
            obj.spec = "5v";
        }
        else {
            PixelColor p3 = getPixel( // Le plus à gauche
                grid.startPos.x - 2 * grid.blocSize,
                grid.startPos.y + 2.5 * grid.blocSize,
                data
            );
            PixelColor p4 = getPixel( // Un peu moins à gauche
                grid.startPos.x - grid.blocSize,
                grid.startPos.y + 2.5 * grid.blocSize,
                data
            );

            if (colorEmpty.matchColor(p3) && !colorEmpty.matchColor(p4)) {
                obj.spec = "4h";
            }
            else if (!colorEmpty.matchColor(p3) && !colorEmpty.matchColor(p4)) {
                obj.spec = "5h";
            }
            else if (colorEmpty.matchColor(p3) && colorEmpty.matchColor(p4)) {
                obj.spec = "No spec";
            }
        }
        cout << "Spec : " << obj.spec;
    }

    return obj;
}


Coord centerObjPosInGrid(const int originalPos, const Obj& obj) {
    const int colOriginBloc = originalPos % NB_COLUMNS; // Colonne dans la grille du premier bloc de l'obj
    const int rowOriginBloc = originalPos / NB_COLUMNS; // Ligne dans la grille du premier bloc de l'obj

    int leftmostColumn = 100;
    int rightmostColumn = -100;
    int topRow = 100;
    int bottomRow = -100;
    
    for (Coord c : obj.coord_format) {
        leftmostColumn = min(leftmostColumn, c.x);
        rightmostColumn = max(rightmostColumn, c.x);

        topRow = min(topRow, c.y);
        bottomRow = max(bottomRow, c.y);
    }

    const float centerColumn = (leftmostColumn + rightmostColumn)/2; // Moyenne : colonne du milieu de l'objet
    const float centerRow = (topRow + bottomRow)/2;                  // Moyenne : ligne du milieu de l'objet

    const float centerColumnInGrid = colOriginBloc + centerColumn;
    const float centerRowInGrid = rowOriginBloc + centerRow;

    const float x = (float)X_START_GRID + (float)BLOCK_SIZE * centerColumnInGrid;  // Convertion en pixel
    const float y = (float)Y_START_GRID + (float)BLOCK_SIZE * centerRowInGrid;     // Convertion en pixel

    return {static_cast<int>(round(x)), static_cast<int>(round(y))};

    /*  La position de l'objet dans la grille (obj) correspond à
        l'emplacement où le coin supérieur gauche de l'objet doit être placé.
        Cette fonction calcule la case de la grille où le milieu de l'objet 
        doit être placé, puis renvoie la position en pixel.
    */
}

void gridToCoord(Obj& o) {
    o.coord_format = {};

    if (o.spec == "4h" || o.spec == "5h" || o.spec == "4v" || o.spec == "5v") {
        o.coord_format = specific_obj.at(o.spec).coord_format;
        printVector(o.coord_format);
        cout << endl;
        return;
    }


    for (int i = 0; i < (int)o.grid_format.size(); i++) {

        const int box_value = o.grid_format.at(i);

        if (box_value == 1) {
            int x = i % NB_COLUMNS_OBJ;
            int y = i / NB_COLUMNS_OBJ;

            if (x % 2 == 0 && y % 2 == 0) {
                x /= 2;
                y /= 2;


                if (o.coord_format.size() >= 1) {
                    x -= o.coord_format.at(0).x;
                    y -= o.coord_format.at(0).y;
                }

                const Coord posBox = {x, y};
                o.coord_format.push_back(posBox);
            }
        }
    }

    o.coord_format.at(0) = {0, 0};

    printVector(o.coord_format);
    cout << endl;
}


vector<bool> reduceGrid(const vector<bool>& originalGrid) {

    vector<vector<bool>> columnsFilling(NB_COLUMNS); // Liste des colonnes
    vector<vector<bool>> rowsFilling(NB_ROWS); // Listes des lignes

    vector<int> filledColumns; // Liste de l'id des colonnes remplies
    vector<int> filledRows; // Liste de l'id des lignes remplies

    vector<bool> clearedGrid = originalGrid;

    for (int i = 0; i < (int)originalGrid.size(); i++) {
        const bool v = originalGrid.at(i);

        const int actCol = i % NB_COLUMNS;
        const int actRow = i / NB_COLUMNS;

        columnsFilling.at(actCol).push_back(v);
        rowsFilling.at(actRow).push_back(v);

        if (actCol == NB_COLUMNS - 1) { // Ce qui signifie que l'on est au bout d'une ligne
            if ( !(find(rowsFilling.at(actRow).begin(), rowsFilling.at(actRow).end(), 0) != rowsFilling.at(actRow).end()) ) { // On ne trouve pas de 0 dans la ligne
                filledRows.push_back(actRow);
            }
        }
        if (actRow == NB_ROWS - 1) { // Ce qui signifie que l'on est au bout d'une colonne
            if ( !(find(columnsFilling.at(actCol).begin(), columnsFilling.at(actCol).end(), 0) != columnsFilling.at(actCol).end()) ) { // On ne trouve pas de 0 dans la colonne
                filledColumns.push_back(actCol);
            }
        }
    }

    for (int j = 0; j < (int)originalGrid.size(); j++) {
        const int actCol = j % NB_COLUMNS;
        const int actRow = j / NB_COLUMNS;

        if ( find(filledColumns.begin(), filledColumns.end(), actCol) != filledColumns.end() ) {
            clearedGrid.at(j) = 0;
        }
        if ( (find(filledRows.begin(), filledRows.end(), actRow) != filledRows.end()) ) {
            clearedGrid.at(j) = 0;
        }
    }

    return clearedGrid;
}

vector<VirtualSolution> putInGrid(const vector<bool>& grid, vector<Obj> listObjs, const vector<int>& orderObjs, const VirtualSolution& originalSlt, bool stopAtFirst) {
    vector<VirtualSolution> listSolution;

    //const vector<int> test = {6, 0};
    //if (originalSlt.posObj == test) {
    //    for (int n = 0; n < (int)originalSlt.finalGrid.size(); n++) {
    //        if (n % 8 == 0 && n != 0) { cout << endl; }
    //        if (originalSlt.finalGrid.at(n)) { cout << " X "; }
    //        else { cout << " . "; }
    //    }
    //
    //    exit(0);
    //}

    for (int iG = 0; iG < (int)grid.size(); iG++) { // iG pour l'index de la case, vG pour sa valeur (1 ou 0)
        const bool vG = grid.at(iG);

        if (vG) continue;

        const int actColumn = iG % NB_COLUMNS + 1;

        bool canPlaceObj = true;
        int posObj = 0;

        vector<bool> newGrid = grid;

        for (int iB = 0; iB < (int)listObjs.at(0).coord_format.size(); iB++) { // iB pour l'index du bloc du 1er objet, vB pour sa valeur (coordonnée)
            const Coord vB = listObjs.at(0).coord_format.at(iB);

            if (vB.x == 0 && vB.y == 0) {
                posObj = iG;
                newGrid.at(iG) = 1;
                continue; // Si les coord sont (0, 0) => 1er bloc de l'obj => on passe au bloc suivant
            }

            const int dcx = vB.x; // dcx = décalage en x
            const int dcy = vB.y; // dcy = décalage en y

            const int posBloc = iG + dcx + NB_ROWS * dcy; // La position du bloc = pos du bloc d'origine de l'objet + décalage en x + décalage en y * nb de ligne

            if (posBloc >= 64 || posBloc < 0 || actColumn + dcx > 8 || actColumn + dcx < 1) {
                canPlaceObj = false;
                break;
            } // Si l'objet dépasse...

            if (grid.at(posBloc) == 0) { // Si le bloc actuel est libre...
                newGrid.at(posBloc) = 1;
            }
            else {
                canPlaceObj = false;
                break;
            }
        }

        if (canPlaceObj) {

            VirtualSolution newSlt = {};

            if (originalSlt.orderObj.empty()) {
                newSlt.orderObj = orderObjs;
            }
            else {
                newSlt = originalSlt;
            }

            newSlt.addObj(reduceGrid(newGrid), posObj);
            listSolution.push_back(newSlt);

            if (stopAtFirst) {
                return listSolution;
            }
        }
    }

    listObjs.erase(listObjs.begin()); // On enlève le dernier élément

    vector<VirtualSolution> listOfEntireSlt = {};

    if (!listObjs.empty()) {
        for (VirtualSolution slt : listSolution) {
            const vector<VirtualSolution> newStepSlt = putInGrid(slt.finalGrid, listObjs, orderObjs, slt, false);
            listOfEntireSlt.insert(listOfEntireSlt.end(), newStepSlt.begin(), newStepSlt.end());
        }

        return listOfEntireSlt;
    }

    return listSolution; // Si il reste des objets, on complète les solutions existantes avec ces objets, sinon, on renvoie la liste.
}

int calculateScore(const vector<bool>& grid, const int& maxScore) {
    int emptyBox = 0; // Nb de cases vides
    int unisolated_emptyBox_4 = 0; // Nb de cases vides non isolées avec 4 voisines vides
    int unisolated_emptyBox_3 = 0; // Nb de cases vides non isolées avec 3 voisines vides
    int unisolated_emptyBox_2 = 0; // Nb de cases vides non isolées avec 2 voisines vides
    int unisolated_emptyBox_1 = 0; // Nb de cases vides non isolées avec 1 voisines vides
    int isolated_emptyBox = 0; // Nb de cases vides isolées

    int isolated_fullBox = 0; // Nb de cases pleines isolées
    int isolated_fullBox_1 = 0; // Nb de cases pleines isolées avec seulement 1 voisine pleine

    for (int iG = 0; iG < (int)grid.size(); iG++) {
        const bool vG = grid.at(iG);

        const bool top = (iG > 7 ? grid.at(iG - NB_COLUMNS) : 1);
        const bool bottom = (iG < 56 ? grid.at(iG + NB_COLUMNS) : 1);

        const int column = iG % NB_COLUMNS;
        const bool left = (column > 0 ? grid.at(iG - 1) : 1);
        const bool right = (column < 7 ? grid.at(iG + 1) : 1);
        // On prend les 4 bloc voisin de la grille

        const int adjactentBox = top + bottom + right + left;

        if (vG == 1) {
            if (adjactentBox == 0) {
                isolated_fullBox += 1;
            }
            else if (adjactentBox == 1) {
                isolated_fullBox_1 += 1;
            }
        }
        else if (vG == 0) {
            emptyBox += 1;

            switch (adjactentBox) {
                case 0:
                    unisolated_emptyBox_4 += 1;
                    break;
                case 1:
                    unisolated_emptyBox_3 += 1;
                    break;
                case 2:
                    unisolated_emptyBox_2 += 1;
                    break;
                case 3:
                    unisolated_emptyBox_1 += 1;
                    break;
                case 4:
                    isolated_emptyBox += 1;
                    break;
            }
        }
    }

    int score = 0;
    score = unisolated_emptyBox_4 * UIB_4_MULT
          + unisolated_emptyBox_3 * UIB_3_MULT
          + unisolated_emptyBox_2 * UIB_2_MULT
          + unisolated_emptyBox_1 * UIB_1_MULT
          + isolated_emptyBox * IB_0_MULT
          + isolated_fullBox * IFB_MULT
          + isolated_fullBox_1 * UIFB_1_MULT;


    if (REFLEXION_STEP == 1 || (maxScore - score) > (O3X3_POINT + O1X5_POINT + O5X1_POINT)) {
        return score;
    }

    const bool canPlace3x3 = putInGrid(grid, {O3X3}, {0}, {}, true).size() > 0;

    score += canPlace3x3 * O3X3_POINT;
    // Points bonus si la grille finale permet de placer un 3x3

    if (REFLEXION_STEP == 2 || (maxScore - score) > (O1X5_POINT + O5X1_POINT)) {
        return score;
    }

    const bool canPlace1x5 = putInGrid(grid, {O1X5}, {0}, {}, true).size() > 0;
    const bool canPlace5x1 = putInGrid(grid, {O5X1}, {0}, {}, true).size() > 0;

    score += canPlace1x5 * O1X5_POINT
          + canPlace5x1 * O5X1_POINT;
    // Points bonus si la grille finale permet de placer un 1x5
    // Points bonus si la grille finale permet de placer un 5x1

    if (REFLEXION_STEP == 3) {
        return score;
    }

    return score;
}


void moveObj(const Coord& pxFrom, const Coord& pxTo) {
    /*  Notre objet doit aller de A0 à B0. 
        Lorsque l'on clique sur l'obj, il est décalé en y (INCREASE_Y).
        Notre objet est donc à la position A1, alors que la souris est en A0.
        La souris doit donc aller en B1 (image de B + INCREASE_Y) pour que l'objet soit en B0.
        En allant de A0 à B1, il y a une accélération de l'objet.
        La souris doit donc aller en pAccel, qui est le point entre A0 et B1 
        qui prend en compte l'accélération pour que l'objet arrive pile sur B0.
    */

    const Coord B1 = {pxTo.x, pxTo.y + INCREASE_Y}; //  Arrivée + le décalage en cliquant sur l'obj = B1

    const int dcx = B1.x - pxFrom.x;
    const int dcy = B1.y - pxFrom.y;

    const int dcxAccel = dcx/kx;
    const int dcyAccel = dcy/ky;

    const Coord pAccel = {pxFrom.x + dcxAccel, pxFrom.y + dcyAccel};

    string swipeCmd = 
    "shell input touchscreen swipe "
    + to_string(pxFrom.x) + " " + to_string(pxFrom.y) + " "
    + to_string(pAccel.x) + " " + to_string(pAccel.y) + " "
    + "1000";

    adb.adb_command(device, swipeCmd);

    cout << "Objet placé ici : x:" << pAccel.x << " y:" << pAccel.y << endl;
}


vector<bool> prog(vector<bool> expectedGrid) {
    SetConsoleOutputCP(CP_UTF8);

    if (!deviceconfigured) {
        device = adb.choose_device();
        cout << endl;

        adb.click(device, 0, 0);

        nameIMG = adb.captureScreen(device, PARENT_IMG_FOLDER);
        pathIMG = PARENT_IMG_FOLDER + nameIMG;

        deviceconfigured = true;

        if (device == "") {
            deviceconfigured = false;
            expectedGrid = {};
            /* Le mode local ne prend pas de capture d'ecran,
             * faire tourner le script plusieurs fois n'est donc pas utile,
             * d'autant plus que les objets n'auront pas été placés, et que
             * le script montrera une erreur : "La grille n'est pas celle attendue..."
             * Une reconfiguration est donc redemandée à chaque fois.
            */
        }
    }
    
    unsigned char* data = stbi_load(pathIMG.c_str(), &width, &height, &channels, 4);

    if (!data) {
        cerr << "Erreur de chargement : " << pathIMG;
        exit(1);
    }


    cout << endl << endl << "Grille : " << endl;
    Obj mainGrid = getGridFilling(
        data,
        gcmain,
        COLOR_EMPTY,
        PRINT_GRID
    );
    cout << endl;

    if (mainGrid.grid_format != expectedGrid && !expectedGrid.empty()) {
        cout << endl << endl << "La grille obtenue n'est pas celle attendue..." << endl;
        cout << "Grille voulue :" << endl;
        printVector(expectedGrid);
        exit(1);
    }

    cout << endl << "Objets : " << endl << endl;

    vector<Obj> listObjs;
    for (int n = 0; n < (int)X_START_OBJ_GRID.size(); n++) {
        cout << "/// " << n << " \\\\\\" << endl;
        Obj o;

        gcobj.startPos.x = X_START_OBJ_GRID.at(n);
        o = getGridFilling(
            data,
            gcobj,
            COLOR_EMPTY_OBJ,
            PRINT_OBJECTS
        );

        if ( find(o.grid_format.begin(), o.grid_format.end(), 1) != o.grid_format.end() ) { // https://stackoverflow.com/questions/571394/how-to-find-out-if-an-item-is-present-in-a-stdvector
            cout << endl;
            gridToCoord(o);
            listObjs.push_back(o);
        }

        cout << endl;
    }

    stbi_image_free(data); // /!\ Important !!! Doit être mis pour éviter les fuites de mémoires

    cout << endl << "Solutions : " << endl;

    vector<int> order = {};
    for (int i = 0; i < (int)listObjs.size(); i++) {
        order.push_back(i);
    }

    auto start_time = chrono::steady_clock::now();

    vector<VirtualSolution> finalList = {};
    vector<vector<Obj>> seen;

    do {
        vector<Obj> listObjsinOrder = {};
        for (int i : order) {
            listObjsinOrder.push_back(listObjs.at(i));
        }

        if (find(seen.begin(), seen.end(), listObjsinOrder) == seen.end()) {
            seen.push_back(listObjsinOrder);

            const VirtualSolution emptySolution = {};
            const vector<VirtualSolution> solutions = putInGrid(mainGrid.grid_format, listObjsinOrder, order, emptySolution, false);

            finalList.insert( finalList.end(), solutions.begin(), solutions.end());
        } // Permet d'éviter de répéter deux fois le calul pour deux situations initiales identiques.

    } while (next_permutation(order.begin(), order.end()));

    auto end_time = chrono::steady_clock::now();
    double elapsedTime = chrono::duration<double>(end_time - start_time).count();

    if (!finalList.empty()) {
        cout << finalList.size() << " solution"
             << ((int)finalList.size() > 1 ? "s " : " ")
             << ((int)finalList.size() > 1 ? "ont " : "a ")
             << "été trouvée"
             << ((int)finalList.size() > 1 ? "s " : " ")
             << "en " << elapsedTime << " seconde"
             << (elapsedTime > 1 ? "s" : "")
             << endl;
        
    }
    else {
        cout << "C'est triste..."
             << endl
             << "Aucune solution n'a été trouvée."
             << endl;

        exit(0);
    }

    



    cout << endl << "Calcul du score : " << endl;
    start_time = chrono::steady_clock::now(); // start_time a déjà été défini plus haut

    vector<int> indexMaxSolution = {};
    int maxScore = -1000000000;
    bool mxscr_slt_PbEG = false; // La meilleure solution passe par une grille vide ?

    for (int i = 0; i < (int)finalList.size(); i++) {
        const VirtualSolution slt = finalList.at(i);
        const int score = calculateScore(slt.finalGrid, maxScore);

        if (score == maxScore) {
            if (slt.passByEmptyGrid == mxscr_slt_PbEG) {
                indexMaxSolution.push_back(i);
            }
            else if (slt.passByEmptyGrid) {
                indexMaxSolution = {i};
                mxscr_slt_PbEG = true;
            }
        }
        else if (score > maxScore) {
            maxScore = score;
            indexMaxSolution = {i};
            mxscr_slt_PbEG = slt.passByEmptyGrid;
        }
    }

    end_time = chrono::steady_clock::now(); // end_time a déjà été défini plus haut
    elapsedTime = chrono::duration<double>(end_time - start_time).count();

    cout << finalList.size() << " solution"
             << ((int)finalList.size() > 1 ? "s " : " ")
             << ((int)finalList.size() > 1 ? "ont " : "a ")
             << "été triée"
             << ((int)finalList.size() > 1 ? "s " : " ")
             << "en " << elapsedTime << " seconde"
             << (elapsedTime > 1 ? "s" : "")
             << endl;

    cout << endl << "Résultats :" << endl;

    VirtualSolution bestSlt = {};
    if ((int)indexMaxSolution.size() > 1) {
        for (int iMS : indexMaxSolution) {
            bestSlt = finalList.at(iMS);

            cout << "Une des meilleures solutions est la n°" << iMS << " :" << endl << '\t'
                 << "ordre: ";
            printVector(bestSlt.orderObj);
            cout << " pos objs: ";
            printVector(bestSlt.posObj);
            cout << " avec un score de " << maxScore << endl;
        }
        bestSlt = finalList.at(indexMaxSolution.at(0)); // Par défaut, on prend la première
    }
    else {
        bestSlt = finalList.at(indexMaxSolution.at(0));
        cout << "La meilleure solution est la n°" << indexMaxSolution.at(0) << " :" << endl << '\t'
             << "ordre: ";
        printVector(bestSlt.orderObj);
            cout << " pos objs: ";
            printVector(bestSlt.posObj);
            cout << " avec un score de " << maxScore << endl;
        
    }

    finalList.clear();

    cout << endl << "Déplacements :" << endl;

    for (int i = 0; i < (int)bestSlt.orderObj.size(); i++) {
        const int j = bestSlt.orderObj.at(i);
        // i va de 0 jusqu'au nb d'objet
        // j est l'index de l'objet qui va être déplacé

        const Obj actualObj = listObjs.at(j);
        const int posActualObj = bestSlt.posObj.at(i);

        const Coord pxFrom = {
            static_cast<int>(round(X_START_OBJ_GRID[j] + 2.5 * BLOCK_SIZE_OBJ)),
            static_cast<int>(round(Y_START_OBJ_GRID + 2.5 * BLOCK_SIZE_OBJ))
        }; // On prend le centre de la grille obj

        const Coord pxTo = { centerObjPosInGrid(posActualObj, actualObj) };

        moveObj(pxFrom, pxTo);
    }

    cout << endl << "Prochaine grille dans :" << endl;
    const float waitTime = 4.f;
    const int accuracy = 100;
    for (int t = 0; t < waitTime * accuracy; t++) {
        this_thread::sleep_for(chrono::milliseconds( 1000 / accuracy ));
        cout << (waitTime - (float)t/accuracy) << '\r';
    }
    cout << "0.00        " << endl << endl;
    

    return bestSlt.finalGrid;
}

int main(int argc, char* argv[]) {
    vector<bool> expectedGrid = {};
    while(true) {
        expectedGrid = prog(expectedGrid);
    }
}