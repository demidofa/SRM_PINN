import numpy as np
def estimate(La, Lb, Lc):
    """Оценка электрического угла по индуктивностям фаз через преобразование Кларк."""
    l_al = (2 * La - Lb - Lc) / 3
    l_be = (Lb - Lc) / np.sqrt(3)
    return np.mod(np.arctan2(-l_be, -l_al), 2 * np.pi)
