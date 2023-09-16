import json
import matplotlib.pyplot as plt
import netCDF4
import numpy as np
import h5py
import os

from scipy.stats import shapiro
from sklearn.cluster import KMeans, AgglomerativeClustering

def compute_shapiro_volume( volumes, mask ):
    shapiro_volume = np.full( volumes.shape[1:], 1.0, dtype=np.float32 )
    
    if volumes.shape[0] < 3:
        return shapiro_volume
    
    for (x, y, z) in np.argwhere( mask ):
        values                  = volumes[:, x, y, z]
        shapiro_volume[x, y, z] = shapiro( values ).pvalue
    return shapiro_volume

def split_ensemble_kmeans( volumes, mask ):
    kmeans      = KMeans( n_clusters=2, n_init="auto", random_state=42 )
    prediction  = kmeans.fit_predict( volumes[:, mask] )
    return prediction.astype( np.bool_ )

def split_ensemble_field_similarity( volumes, mask ):
    member_count        = volumes.shape[0]
    field_similarity    = np.zeros( (member_count, member_count) )
    
    minima              = np.min( volumes[:, mask], axis=1 )
    maxima              = np.max( volumes[:, mask], axis=1 )
    
    for i in range( member_count ):
        print( f"\rfield similarity: {i}", end="", flush=True )
        for j in range( i + 1, member_count ):
            domain_minimum          = min( minima[[i, j]] )
            domain_range            = max( maxima[[i, j]] ) - domain_minimum
            
            values                  = volumes[[i, j]][:, mask]
            minimum_field           = ( np.min( values, axis=0 ) - domain_minimum ) / domain_range
            maximum_field           = ( np.max( values, axis=0 ) - domain_minimum ) / domain_range
            
            similarity              = 1.0 - ( np.sum( 1.0 - maximum_field ) / np.sum( 1.0 - minimum_field ) )
            
            field_similarity[i, j]  = field_similarity[j, i] = similarity
    print()
    
    agglomerative_clustering    = AgglomerativeClustering( n_clusters=2, metric="precomputed", linkage="complete" )
    prediction                  = agglomerative_clustering.fit_predict( field_similarity )
    return prediction.astype( np.bool_ )

def split_ensemble_ranking( volumes, mask ):    
    values          = volumes[:, mask]
    argsorted       = np.argsort( values, axis=0 )
    scores          = np.arange( volumes.shape[0], dtype=np.uint64 )
    ranks           = np.zeros( (volumes.shape[0]), dtype=np.uint64 )
    
    for i in range( argsorted.shape[1] ):
        ranks[argsorted[:, i]] += scores
    
    argsorted           = np.argsort( ranks )    
    best_score          = -1
    best_split_index    = -1
    
    for i in range( 3, argsorted.shape[0] - 2 ):
        completion_percentage   = 100.0 * ( i - 2 ) / ( argsorted.shape[0] - 5 )
        print( f"\rComputing best rank-based ensemble split... ({completion_percentage:.1f} %)", end="", flush=True )
        
        first_indices           = argsorted[:i]
        second_indices          = argsorted[i:]
        
        first_shapiro_volume    = compute_shapiro_volume( volumes[first_indices], mask )
        second_shapiro_volume   = compute_shapiro_volume( volumes[second_indices], mask )
        score                   = np.sum( first_shapiro_volume >= 0.05 ) * len( first_indices ) + np.sum( second_shapiro_volume >= 0.05 ) * len( second_indices )
        
        if score > best_score:
            best_score          = score
            best_split_index    = i
    print( "" )
    
    prediction                                  = np.zeros( argsorted.shape[0], dtype=np.bool_ )
    prediction[argsorted[best_split_index:]]    = True
    return prediction

def compute_sub_ensembles( filename, volumes, indices, mask, algorithm, level, max_level ):
    print( f"[ ---------- '{filename}' ---------- ]" )
    
    if os.path.isfile( f"{filename}_shapiro.npy" ):
        shapiro_volume      = np.load( f"{filename}_shapiro.npy" )
        normality           = mask & ( shapiro_volume >= 0.05 )
        remaining_mask      = mask & ~normality
        
    else:
        print( f"Computing shapiro volume for '{filename}'... ", end="", flush=True )
        shapiro_volume      = compute_shapiro_volume( volumes[indices], mask )
        normality           = mask & ( shapiro_volume >= 0.05 )
        remaining_mask      = mask & ~normality
        print( "Done!" )
        
        print( f"Saving files for '{filename}'... ", end="", flush=True )
        np.save( f"{filename}_indices.npy", indices )
        np.save( f"{filename}_mask.npy", mask )
        np.save( f"{filename}_shapiro.npy", shapiro_volume )
        print( "Done!" )
    
    total_member_count      = volumes.shape[0]
    indices_member_count    = len( indices )
    total_voxel_count       = np.prod( volumes.shape[1:] )
    mask_voxel_count        = np.sum( mask )
    normal_voxel_count      = np.sum( normality )    
    remaining_voxel_count   = np.sum( remaining_mask )
    
    print( f" -> members:   {indices_member_count} / {total_member_count} ({100.0 * indices_member_count / total_member_count:.1f} %)" )
    print( f" -> voxels:    {mask_voxel_count} / {total_voxel_count} ({100.0 * mask_voxel_count / total_voxel_count:.1f} %)" )
    print( f" -> normal:    {normal_voxel_count} / {mask_voxel_count} ({100.0 * normal_voxel_count / mask_voxel_count:.1f} %)" )    
    print( f" -> remaining: {remaining_voxel_count} / {mask_voxel_count} ({100.0 * remaining_voxel_count / mask_voxel_count:.1f} %)" )
    print()
    
    if indices_member_count >= 6 and remaining_voxel_count >= 1 and level < max_level:
        split   = algorithm["function"]( volumes[indices], remaining_mask )
        first   = indices[split]
        second  = indices[~split]
        
        if level == 0:
            filename  = f"{filename}{algorithm['identifier']}/"
            if not os.path.isdir( filename ):
                os.mkdir( filename )
        
        compute_sub_ensembles( f"{filename}_A", volumes, first, remaining_mask, algorithm, level + 1, max_level )
        compute_sub_ensembles( f"{filename}_B", volumes, second, remaining_mask, algorithm, level + 1, max_level )   

def load_volumes_red_sea( field ):
    base_directory  = "./datasets/red_sea/0001"
    
    volumes         = []
    for filename in os.listdir( base_directory ):
        file        = h5py.File( f"{base_directory}/{filename}", "r" )
        
        if field == "magnitude":
            u       = file["U"][()].astype( np.float32 )
            v       = file["V"][()].astype( np.float32 )
            w       = file["W"][()].astype( np.float32 )
            volume  = np.sqrt( u * u + v * v + w * w )
            
        else:        
            volume  = file[field][()].astype( np.float32 )
        
        volumes.append( volume )
        
    volumes         = np.stack( volumes )
    mask            = volumes[0] != 0.0
    
    return (volumes, mask)

def load_volumes_rfa():    
    file            = open( "./datasets/radio_frequency_ablation.bin" )
    shape           = np.fromfile( file, dtype=np.uint32, count=4 )
    volumes         = np.fromfile( file, dtype=np.float32 ).reshape( shape )
    mask            = np.full( volumes.shape[1:], True, dtype=np.bool_ )

    return volumes, mask
    
def evaluate_sub_ensembles( filename, method, level=0, total_member_count=None, total_voxel_count=None, max_level=512 ):
    if not os.path.isfile( f"{filename}_shapiro.npy" ):
        return None
    
    indices             = np.load( f"{filename}_indices.npy" )
    mask                = np.load( f"{filename}_mask.npy" )
    shapiro_volume      = np.load( f"{filename}_shapiro.npy" )
    normality           = mask & ( shapiro_volume >= 0.05 )
    remaining_mask      = mask & ~normality
    
    member_count            = len( indices )
    mask_voxel_count        = int( np.sum( mask ) )
    normal_voxel_count      = int( np.sum( normality ) )
    remaining_voxel_count   = int( np.sum( remaining_mask ) )
    
    if level == 0:
        filename            = f"{filename}{method}/"
        total_member_count  = member_count
        total_voxel_count   = mask_voxel_count
        
    first = None if level >= max_level else evaluate_sub_ensembles( f"{filename}_A", method, level + 1, total_member_count, total_voxel_count, max_level )
    if first is None:
        first = (np.zeros_like( normality ), { "total_explainability": 0.0 })
    
    second = None if level >= max_level else evaluate_sub_ensembles( f"{filename}_B", method, level + 1, total_member_count, total_voxel_count, max_level )
    if second is None:
        second = (np.zeros_like( normality ), { "total_explainability": 0.0 })    
    
    evaluation = {
        "member_count":                     member_count,
        "member_percentage":                member_count / total_member_count,
        "voxel_count":                      mask_voxel_count,
        "voxel_total_percentage":           mask_voxel_count / total_voxel_count,
        "normal_voxel_count":               normal_voxel_count,
        "normal_voxel_mask_percentage":     normal_voxel_count / mask_voxel_count,
        "normal_voxel_total_percentage":    normal_voxel_count / total_voxel_count,
        "split_a":                          first[1],
        "split_b":                          second[1]
    }
    cumulative_normality                            = normality | ( first[0] & second[0] )
    evaluation["fully_explained_voxel_count"]       = int( np.sum( cumulative_normality ) )
    evaluation["fully_explained_voxel_percentage"]  = evaluation["fully_explained_voxel_count"] / total_voxel_count
    evaluation["explainability"]                    = evaluation["normal_voxel_total_percentage"] * evaluation["member_percentage"]
    evaluation["total_explainability"]              = evaluation["explainability"] + first[1]["total_explainability"] + second[1]["total_explainability"]    
    return (cumulative_normality, evaluation)

def show_explainability_plot():
    xs      = [0, 1, 2, 3, 4]
    xlabels = [1, 3, 7, 15, 31]
    
    directories = ["radio_frequency_ablation/", "red_sea_0001_temperature/", "red_sea_0001_salinity/", "red_sea_0001_magnitude/"]
    identifiers = ["kmeans", "field_similarity", "ranking"]
    
    plt.rcParams.update({'font.size': 20})
    fig, axis = plt.subplots( 1, 1 )
    
    for directory in directories:
        for identifier in identifiers:
            ys = [evaluate_sub_ensembles( directory, identifier, max_level=max_level )[1]["total_explainability"] for max_level in xs]            
            axis.plot( xs, ys, label=f"{directory} ({identifier})" )
            
    axis.set_xticks( xs, xlabels )
    axis.set_xlabel( "Number of Sub-Ensembles" )
    axis.set_ylabel( "Explainability" )
    
    plt.show()

if __name__ == "__main__":
    algorithms = [
        { "function": split_ensemble_kmeans,            "identifier": "kmeans" },
        { "function": split_ensemble_field_similarity,  "identifier": "field_similarity" },
        { "function": split_ensemble_ranking,           "identifier": "ranking" }
    ]
    # directory               = "radio_frequency_ablation/"
    directory               = "red_sea_0001_temperature/"
    # directory               = "red_sea_0001_salinity/"
    # directory               = "red_sea_0001_magnitude/"
    
    for algorithm in algorithms:
        # volumes, mask   = load_volumes_rfa()
        volumes, mask   = load_volumes_red_sea( "TEMP" )
        # volumes, mask   = load_volumes_red_sea( "SALT" )
        # volumes, mask   = load_volumes_red_sea( "magnitude" )
        indices         = np.arange( volumes.shape[0] )
        
        if not os.path.isdir( directory ):
            os.mkdir( directory )
            
        compute_sub_ensembles( directory, volumes, indices, mask, algorithm, level=0, max_level=4 )
        
        _, evaluation           = evaluate_sub_ensembles( directory, algorithm["identifier"] )
        evaluation_filename     = f"{directory}{algorithm['identifier']}_evaluation.json"
        json.dump( evaluation, open( evaluation_filename, "w" ), indent=4, sort_keys=True )
        print( json.dumps( evaluation, indent=4, sort_keys=True ) )