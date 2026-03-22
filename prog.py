import subprocess
import time

from PIL import Image
from itertools import permutations
from copy import deepcopy
from tqdm import tqdm
from concurrent.futures import ProcessPoolExecutor, as_completed

from list_blocks import blocks, specific_blocks

# Reflexion
MAX_ASYNC_TASK = 3

REFLEXION_STEP = 3
UIB_4_MULT = 3 # Multiplicateur pour les cases vides avec 4 voisines vides
UIB_3_MULT = 2 # Multiplicateur pour les cases vides avec 3 voisines vides
UIB_2_MULT = -1 # Multiplicateur pour les cases vides avec 2 voisines vides
UIB_1_MULT = -1.5 # Multiplicateur pour les cases vides avec 1 voisine vide
IB_0_MULT = -2 # Multiplicateur pour les cases vides avec 0 voisine vide

IFB_MULT = -5 # Multiplicateur pour les cases pleines avec 0 voisines pleines
UIFB_1_MULT = -4 # Multiplicateur pour les cases pleines avec seulement 1 voisine pleine

B3X3 = ((0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (2, 1), (0, 2), (1, 2), (2, 2))
B3X3_POINT = 25
B1X5 = ((0, 0), (0, 1), (0, 2), (0, 3), (0, 4))
B1X5_POINT = 15
B5X1 = ((0, 0), (1, 0), (2, 0), (3, 0), (4, 0))
B5X1_POINT = 15

# Déplacements
INCREASE_Y = 308
ky = 1.33
kx = 1.3

# Grille
X_START_GRID = 120
Y_START_GRID = 610

NB_ROWS = 8
NB_COLUMNS = 8
BLOCK_SIZE = 120

COLOR_EMPTY = (25, 36, 66, 255)
TOLERANCE_GRID = 10
PRINT_GRID = True

# Grilles des objets
X_START_OBJ_GRID = (158.5, 471.5, 786.5)
Y_START_OBJ_GRID = 1721.5

NB_ROWS_OBJ = 6
NB_COLUMNS_OBJ = 6
BLOCK_SIZE_OBJ = 27

COLOR_EMPTY_OBJ = (58, 81, 148, 255)
TOLERANCE_OBJ_GRID = 25
PRINT_OBJECTS = True

# Objets spécifiques
i_horizontal_B3 = 0
i_vertical_B3 = 1

# Classe des solutions virtuelles
class VirtualSolution:
    def __init__(self, order_obj):
        self.grid = ()

        self.pos_obj = ()
        self.pass_by_empty_grid = False
        self.order = order_obj

    def addObj(self, new_grid, pos_obj):
        self.grid = new_grid
        self.pos_obj = self.pos_obj + (pos_obj,)
        self.pass_by_empty_grid = self.pass_by_empty_grid or 1 not in new_grid



# Tout ce qui est lié à l'écran
def capture_screen():
    subprocess.check_output(["adb", "shell", "input", "tap", "0", "0"])
    data = subprocess.check_output(["adb", "exec-out", "screencap", "-p"])

    with open("screen.png", "wb") as f:
        f.write(data)

def get_color_pixel(x, y, screen):
    return screen.getpixel((x, y))

def move_block(pixel_from, pixel_to):
    # Notre objet doit aller de A0 à B0. 
    # Lorsque l'on clique sur l'obj, il est décalé en y (INCREASE_Y).
    # Notre objet est donc à la position A1, alors que la souris est en A0.
    # La souris doit donc aller en B1 (image de B + INCREASE_Y) pour que l'objet soit en B0.
    # En allant de A à B1, il y a une accélération de l'objet.
    # La souris doit donc aller en p_accel, qui est le point entre A et B1 
    # qui prend en compte l'accélération pour que l'objet arrive pile sur B.
    
    B1 = (pixel_to[0], pixel_to[1] + INCREASE_Y)    # Arrivée - le décalage en cliquant sur l'obj = B1

    dcx = B1[0] - pixel_from[0]
    dcy = B1[1] - pixel_from[1]

    dcx_accel = dcx/kx
    dcy_accel = dcy/ky

    p_accel = (pixel_from[0] + dcx_accel, pixel_from[1] + dcy_accel)

    output = subprocess.check_output([
        "adb", "shell", "input", "touchscreen", "swipe",
        str(pixel_from[0]), str(pixel_from[1]),
        str(p_accel[0]), str(p_accel[1]),
        "1000"
    ])

    print(f"Objet placé ici : x: {p_accel[0]} y: {p_accel[1]}\n\tdcy: {dcy} dcx: {dcx}")

def get_grid_filling(screen, columns, rows, start_x, start_y, b_size, color_bloc_empty, tolerance, to_print):
    filling = ()
    for i in range(columns * rows):
        act_row = i // columns
        act_column = i % columns

        x = start_x + b_size * act_column
        y = start_y + b_size * act_row

        r1, g1, b1, a1 = get_color_pixel(x, y, screen)
        r2, g2, b2, a2 = color_bloc_empty
        is_empty =  abs(r1 - r2) < tolerance and abs(g1 - g2) < tolerance and abs(b1 - b2) < tolerance        

        if columns == 1 or columns == 2:
            print(f"[{i + 1}] X: {x}, Y: {y}, empty?: {is_empty}")

        filling = filling + (not is_empty,)

        if to_print:
            if i % columns == 0 and i != 0:
                print("")

            if is_empty:
                print(" . ", end="")
            else :
                print(" X ", end="")
    return filling


# Tout ce qui est lié aux calculs
def put_in_grid(grid, list_obj, order_obj, original_solution, stop_at_first):
    list_solution = ()

    for iG, vG in enumerate(grid): # iG pour l'index de la case, vG pour sa valeur (1 ou 0)
        if vG == 1:
            continue

        act_column = iG % NB_COLUMNS + 1

        can_place_obj = True
        pos_obj = 0
        new_grid = list(grid).copy()

        for iB, vB in enumerate(list_obj[0]): # iB pour l'index du bloc du 1er objet, vB pour sa valeur (tuple de coordonnée)
            if vB[0] == 0 and vB[1] == 0:
                pos_obj = iG
                new_grid[iG] = 1
                continue # Si le tuple est (0, 0) => 1er tuple de l'obj => on passe au tuple suivant

            dcx = vB[0] # dcx = décalage en x = 1ère valeur du tuple
            dcy = vB[1] # dcy = décalage en y = 2ème valeur du tuple

            posBloc = iG + dcx + NB_ROWS * dcy # La position du bloc = pos du bloc d'origine de l'objet + décalage en x + décalage en y * nb de ligne

            if posBloc >= 64 or posBloc < 0 or act_column + dcx > 8 or act_column + dcx < 1:
                can_place_obj = False
                break

            if grid[posBloc] == 0:
                new_grid[posBloc] = 1
            else:
                can_place_obj = False
                break

        if can_place_obj:
            if original_solution == None:
                new_solution = VirtualSolution(order_obj)
            else:
                new_solution = deepcopy(original_solution)

            new_solution.addObj(reduce_grid(new_grid), pos_obj)
            list_solution = list_solution + (new_solution,)

            if stop_at_first:
                return list_solution

        
    new_list_obj = list_obj[1:]

    complete_slt = ()
    if len(new_list_obj) > 0:
        for slt in list_solution:
            complete_slt = complete_slt + put_in_grid(slt.grid, new_list_obj, order_obj, slt, False)

        list_solution = ()
        list_solution = deepcopy(complete_slt)

    return list_solution

def reduce_grid(grid):
    columns_filling = [[] for _ in range(NB_COLUMNS)] # Créer une liste avec NB_COLUMNS liste vide dedans
    rows_filling =  [[] for _ in range(NB_ROWS)] # Créer une liste avec NB_ROWS liste vide dedans

    filled_columns = []
    filled_rows = []

    new_grid = grid.copy()

    for iG, vG in enumerate(grid): # iG pour l'index de la case, vG pour sa valeur (1 ou 0)
        act_column = iG % NB_COLUMNS
        act_row = iG // NB_COLUMNS

        columns_filling[act_column].append(vG)
        rows_filling[act_row].append(vG)

    for iC, vC in enumerate(columns_filling): # iC pour l'index de la colonne, vC pour sa valeur (liste de bloc)
        full_columns = True

        for bloc in vC:
            if bloc == 0:
                full_columns = False
                break
        
        if full_columns == True:
            filled_columns.append(iC)

    for iR, vR in enumerate(rows_filling): # iR pour l'index de la colonne, vR pour sa valeur (liste de bloc)
        full_rows = True

        for bloc in vR:
            if bloc == 0:
                full_rows = False
                break
        
        if full_rows == True:
            filled_rows.append(iR)

    for iG, vG in enumerate(grid): # iG pour l'index de la case, vG pour sa valeur (1 ou 0)
        act_column = iG % NB_COLUMNS
        act_row = iG // NB_ROWS

        if act_column in filled_columns or act_row in filled_rows:
            new_grid[iG] = 0

    return tuple(new_grid)

def calculate_score(solution, max_score):
    eBox = 0 # Nb de cases vides
    unisolated_4_eBox = 0 # Nb de cases vides non isolées avec 4 voisines vides
    unisolated_3_eBox = 0 # Nb de cases vides non isolées avec 3 voisines vides
    unisolated_2_eBox = 0 # Nb de cases vides non isolées avec 2 voisines vides
    unisolated_1_eBox = 0 # Nb de cases vides non isolées avec 1 voisines vides
    isolated_eBox = 0 # Nb de cases vides isoléees

    isolated_fBox = 0 # Nb de cases pleines isolées
    isolated_1_fBox = 0 # Nb de cases pleines isolées avec seulement 1 voisine pleine

    for iG, vG in enumerate(solution.grid):

        top = solution.grid[iG - NB_COLUMNS] if iG > 7 else 1
        bottom = solution.grid[iG + NB_COLUMNS] if iG < 56 else 1
        right = solution.grid[iG + 1] if iG % NB_COLUMNS < 7 else 1
        left = solution.grid[iG - 1] if iG % NB_COLUMNS > 0 else 1 # On prend les 4 bloc voisins

        adjacent_box = top + bottom + right + left

        if vG == 1:
            if adjacent_box == 0:
                isolated_fBox += 1

            elif adjacent_box ==1:
                isolated_1_fBox
            
        elif vG == 0:
            eBox += 1
            
            if adjacent_box == 0:
                unisolated_4_eBox += 1

            elif adjacent_box == 1:
                unisolated_3_eBox += 1

            elif adjacent_box == 2:
                unisolated_2_eBox += 1

            elif adjacent_box == 3:
                unisolated_1_eBox += 1

            elif adjacent_box == 4:
                isolated_eBox += 1
    
    score = 0
    score = unisolated_4_eBox * UIB_4_MULT + unisolated_3_eBox * UIB_3_MULT + unisolated_2_eBox * UIB_2_MULT + unisolated_1_eBox * UIB_1_MULT + isolated_eBox * IB_0_MULT
    score += isolated_fBox * IFB_MULT + isolated_1_fBox * UIFB_1_MULT

    if REFLEXION_STEP == 1 or (max_score - score) > (B3X3_POINT + B1X5_POINT + B5X1_POINT):
        return score
    
    can_place_3x3 = len(put_in_grid(solution.grid, (B3X3,), 0, None, True)) > 0

    score += can_place_3x3 * B3X3_POINT

    if REFLEXION_STEP == 2 or (max_score - score) > (B1X5_POINT + B5X1_POINT):
        return score

    can_place_1x5 = len(put_in_grid(solution.grid, (B1X5,), 0, None, True)) > 0
    can_place_5x1 = len(put_in_grid(solution.grid, (B5X1,), 0, None, True)) > 0

    score += can_place_1x5 * B1X5_POINT + can_place_5x1 * B5X1_POINT

    if REFLEXION_STEP == 3:
        return score

    return score


# Tout ce qui est lié aux objets
def center_the_block(original_pos, obj):
    col_bloc = original_pos % NB_COLUMNS
    row_bloc = original_pos // NB_COLUMNS

    min_col_obj = min(b[0] for b in obj)
    max_col_obj = max(b[0] for b in obj)
    nb_col_obj = max_col_obj - min_col_obj + 1

    min_row_obj = min(b[1] for b in obj)
    max_row_obj = max(b[1] for b in obj)
    nb_row_obj = max_row_obj - min_row_obj + 1

    center_col = (min_col_obj + max_col_obj)/2
    center_row = (min_row_obj + max_row_obj)/2 

    col_center_bloc = col_bloc + center_col
    row_center_bloc = row_bloc + center_row

    x = X_START_GRID + BLOCK_SIZE * col_center_bloc
    y = Y_START_GRID + BLOCK_SIZE * row_center_bloc

    return (x, y)

def convert_to_coord(obj):
    pos = []

    if obj == "4h" or obj == "4v" or obj == "5h" or obj == "5v":
        for spec_obj in specific_blocks:
            if spec_obj["name"] == obj:
                pos = spec_obj["pos"]
        print(f"\n{pos}")
        return tuple(pos)
    
    for i, v in enumerate(obj): # i est l'index et v la valeur
        if v == 1:
            x = i % 6
            y = i // 6

            if x % 2 == 0 and y % 2 == 0:
                x = x/2
                y = y/2
                if len(pos) >= 1:
                    x -= pos[0][0] # On enlève les coord de départ pour
                    y -= pos[0][1] # avoir des coord relatives au point de départ
                pos.append( (int(x), int(y)) ) # Ajoute le tuple

    # Explications :
    # On parcour la forme, jusqu'a trouver un 1
    # On calcule la colonne et la ligne
    # On continue uniquement si la colonne et la ligne sont paires
    # On divise par deux pour ne pas prendre en compte les colonnes et lignes impaires

    # Tout cela fait que l'on passe d'une forme 6x6,
    # où chaque pixel analysé est un demi bloc,
    # à une forme 3x3, comme les vrais blocs du jeux
    
    pos.pop(0)
    pos.insert(0, (0, 0))
    print(f"\n{tuple(pos)}")

    return tuple(pos)

def verify_bar_3(obj):
    return obj == specific_blocks[i_horizontal_B3]["shape"] or obj == specific_blocks[i_vertical_B3]["shape"]


# Main
def main(grid):
    main_grid = ()
    objects = []
    coords_obj = ()
    capture_screen()
    screen = Image.open("screen.png")

    print("\nGrille :\n")
    main_grid = get_grid_filling(
        screen, 
        NB_COLUMNS, 
        NB_ROWS, 
        X_START_GRID, 
        Y_START_GRID, 
        BLOCK_SIZE, 
        COLOR_EMPTY,
        TOLERANCE_GRID,
        PRINT_GRID
    )

    if main_grid != grid and grid != None:
        print("\n\nLa grille n'est pas celle attendue...\nVoulue : ")
        grid_char = []
        for iB, vB in enumerate(grid):
            act_column = iB % NB_COLUMNS
            if act_column % NB_COLUMNS == 0:
                print("")

            if vB == 1:
                print(" X ", end="")
            else:
                print(" . ", end="")
        return

    print("\n")
    print(f"Len : {len(main_grid)}")

    print("\nObjects :\n")
    for n in range(len(X_START_OBJ_GRID)):
        print(f"/// {n} ///")
        objects.append(get_grid_filling(
            screen,
            NB_COLUMNS_OBJ,
            NB_ROWS_OBJ,
            X_START_OBJ_GRID[n],
            Y_START_OBJ_GRID,
            BLOCK_SIZE_OBJ,
            COLOR_EMPTY_OBJ,
            TOLERANCE_OBJ_GRID,
            PRINT_OBJECTS
        ))

        if verify_bar_3(objects[n]):
            print("\nBar3 detected...\n", end="")
            additional_px = get_grid_filling(
                screen,
                1,
                2,
                (X_START_OBJ_GRID[n] + 2.5 * BLOCK_SIZE_OBJ),
                (Y_START_OBJ_GRID - 2 * BLOCK_SIZE_OBJ),
                BLOCK_SIZE_OBJ,
                COLOR_EMPTY_OBJ,
                TOLERANCE_OBJ_GRID,
                False
            )
            if additional_px == (False, True):
                print("Spec : 4v", end="")
                objects[n] = "4v"
            elif additional_px == (True, True):
                print("Spec : 5v", end="")
                objects[n] = "5v"
            elif additional_px == (False, False):
                additional_px = get_grid_filling(
                    screen,
                    2,
                    1,
                    (X_START_OBJ_GRID[n] - 2 * BLOCK_SIZE_OBJ),
                    (Y_START_OBJ_GRID + 2.5 * BLOCK_SIZE_OBJ),
                    BLOCK_SIZE_OBJ,
                    COLOR_EMPTY_OBJ,
                    TOLERANCE_OBJ_GRID,
                    False
                )
                if additional_px == (False, True):
                    print("Spec : 4h", end="")
                    objects[n] = "4h"
                elif additional_px == (True, True):
                    print("Spec : 5h", end="")
                    objects[n] = "5h"
                elif additional_px == (False, False):
                    print("Spec : no-spec", end="")
                else :
                    print(f"hmph hmph grr... {additional_px}")
            else :
                print(f"hmph... {additional_px}")

        if isinstance(objects[n], str) or 1 in objects[n]:
            coords_obj = coords_obj + (convert_to_coord(objects[n]),)
        print("\n")
    




    print("\nSolutions :")
    seen = set()
    perms = []
    for order in permutations(range(len(coords_obj))): # Créer toutes les permutations
        objs = tuple(coords_obj[i] for i in order)

        if objs not in seen:
            seen.add(objs)

            perms.append((main_grid, objs, order, None, False))

    perms_batches = [ # Sépare les permutations pour le multi processoring
        perms[i:i+MAX_ASYNC_TASK]
        for i in range(0, len(perms), MAX_ASYNC_TASK)
    ]
    
    start_time = time.time()
    pbar = tqdm(total=len(perms), leave=False)

    final_list = ()
    with ProcessPoolExecutor(max_workers=MAX_ASYNC_TASK) as executor: # Multi processoring
        for b in perms_batches:
            futures = [
                executor.submit(put_in_grid, *args)
                for args in b
            ]
    
            for f in as_completed(futures):
                final_list = final_list + f.result()
                pbar.update(1)
    
    end_time = time.time()
    pbar.close()

    if len(final_list) > 0:
        print(f"{len(final_list)} solution{"s" if len(final_list) > 1 else ""} {"ont" if len(final_list) > 1 else "a"} été trouvée{"s" if len(final_list) > 1 else ""} en {round(end_time - start_time, 3)} secondes")
    else:
        print("C'est triste... \nAucune solution n'a été trouvée.")
        return





    print("\nCalcul du score :")
    start_time = time.time()
    pbar = tqdm(total=len(final_list), leave=False)

    do_exit = False
    index_max_slt = 0
    max_score = -1_000_000_000
    ms_slt_pass_by_empty_grid = False
    for iS, slt in enumerate(final_list):
        score = calculate_score(slt, max_score)
        pbar.update(1)

        if score == max_score:
            if ms_slt_pass_by_empty_grid == slt.pass_by_empty_grid:
                index_max_slt = index_max_slt + (iS,)
            elif slt.pass_by_empty_grid:
                index_max_slt = (iS,)
                ms_slt_pass_by_empty_grid = True
        if score > max_score:
            max_score = score
            ms_slt_pass_by_empty_grid = slt.pass_by_empty_grid
            index_max_slt = (iS,)

    end_time = time.time()
    pbar.close()

    print(f"{len(final_list)} solutions ont été triées en {round(end_time - start_time, 3)} secondes")

    print("\nRésultats :")

    best_slt = None
    if len(index_max_slt) > 1:
        for iMS in index_max_slt:
            best_slt = final_list[iMS]
            print(f"Une des meilleures solutions est la n°{iMS}: \n\t{best_slt} -- ({best_slt.pos_obj} / {best_slt.order}) ==> {max_score} + {best_slt.pass_by_empty_grid}\n")
        best_slt = final_list[index_max_slt[0]] # Par défaut, on prend la première
    else :
        best_slt = final_list[index_max_slt[0]]
        print(f"La meilleure solution est la n°{index_max_slt[0]} : \n\t{best_slt} -- ({best_slt.pos_obj} / {best_slt.order}) ==> {max_score} + {best_slt.pass_by_empty_grid}\n")

    final_list = []

    print("\nDéplacements :")

    for i in range(len(coords_obj)):
        obj = coords_obj[best_slt.order[i]]

        pos_bloc = best_slt.pos_obj[i]

        move_block(
            (
                (
                    X_START_OBJ_GRID[best_slt.order[i]] + 2.5 * BLOCK_SIZE_OBJ,
                    Y_START_OBJ_GRID + 2.5 * BLOCK_SIZE_OBJ
                )
            ),
            (
                center_the_block(pos_bloc, obj)
            )
        )
    
    print("\nProchaine grille dans :")
    wait_time = 4.0
    for t in range(int(wait_time) * 100):
        time.sleep(wait_time/(int(wait_time) * 100))
        print(round(wait_time - t/100.0, 2), end="\r")
    main(best_slt.grid)

if __name__ == "__main__":
    main(None)