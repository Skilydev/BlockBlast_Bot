specific_blocks = [
    {
        "name": "horizontalBar3",
        "shape": (
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0
        )
    },
    {
        "name": "verticalBar3",
        "shape": (
            0, 0, 1, 1, 0, 0,
            0, 0, 1, 1, 0, 0,
            0, 0, 1, 1, 0, 0,
            0, 0, 1, 1, 0, 0,
            0, 0, 1, 1, 0, 0,
            0, 0, 1, 1, 0, 0
        )
    },
    {
        "name": "4h",
        "pos": ((0, 0), (1, 0), (2, 0), (3, 0))
    },
    {
        "name": "4v",
        "pos": ((0, 0), (0, 1), (0, 2), (0, 3))
    },
    {
        "name": "5h",
        "pos": ((0, 0), (1, 0), (2, 0), (3, 0), (4, 0))
    },
    {
        "name": "5v",
        "pos": ((0, 0), (0, 1), (0, 2), (0, 3), (0, 4))
    }
]

indexForHB3 = 0
indexForVB3 = 1

def convertToCoord(obj):
    pos = []

    if obj == "4h" or obj == "4v" or obj == "5h" or obj == "5v":
        pos = posInSpecificBlocks(obj)
        print(f"\n{pos}")
        return pos
    
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
    print(f"\n{pos}")

    return tuple(pos)



def verifyBar3(obj):
    return obj == specific_blocks[indexForHB3]["shape"] or obj == specific_blocks[indexForVB3]["shape"]

def posInSpecificBlocks(name):
    for obj in specific_blocks:
        if obj["name"] == name:
            return obj["pos"]
    return None