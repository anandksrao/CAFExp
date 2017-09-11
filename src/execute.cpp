#include <cmath>

#include "utils.h"
#include "io.h"
#include "execute.h"
#include "probability.h"
#include "lambda.h"
#include "core.h"

//! Read user provided gene family data (whose path is stored in input_parameters instance)
std::vector<gene_family> * execute::read_gene_family_data(const input_parameters &my_input_parameters, int &max_family_size, int &max_root_family_size) {
    
    std::vector<gene_family> *p_gene_families = NULL;
    
    if (!my_input_parameters.input_file_path.empty()) {
        p_gene_families = read_gene_families(my_input_parameters.input_file_path);
            
        // Iterating over gene families to get max gene family size
        for (std::vector<gene_family>::iterator it = p_gene_families->begin(); it != p_gene_families->end(); ++it) {
            int this_family_max_size = it->get_max_size();
            
            if (max_family_size < this_family_max_size)
                max_family_size = this_family_max_size;
        }

        max_root_family_size = std::max(30, static_cast<int>(std::rint(max_family_size*1.25)));
        max_family_size = max_family_size + std::max(50, max_family_size/5);
        cout << "Max family size is: " << max_family_size << endl;
        cout << "Max root family size is: " << max_root_family_size << endl;
            
        }

    else { p_gene_families = new std::vector<gene_family>(); }
    
    return p_gene_families;
}

//! Read user provided phylogenetic tree (whose path is stored in input_parameters instance)
clade * execute::read_input_tree(const input_parameters &my_input_parameters) {
    clade *p_tree = new clade();
    p_tree = read_tree(my_input_parameters.tree_file_path, false);
    
    return p_tree;
}

//! Read user provided lambda tree (lambda structure)
clade * execute::read_lambda_tree(const input_parameters &my_input_parameters) {
    clade * p_lambda_tree = NULL; // not new clade() because read_tree() below also allocates memory (so this would leak memory)
    
    if (!my_input_parameters.lambda_tree_file_path.empty()) {
        p_lambda_tree = read_tree(my_input_parameters.lambda_tree_file_path, true);
        // node_name_to_lambda_index = p_lambda_tree->get_lambda_index_map();
        // p_lambda_tree->print_clade();
        }
    
    return p_lambda_tree;
}

//! Read user provided single or multiple lambdas
lambda * execute::read_lambda(const input_parameters &my_input_parameters, probability_calculator &my_calculator, clade * p_lambda_tree) {
      
    lambda *p_lambda = NULL; // lambda is an abstract class, and so we can only instantiate it as single_lambda or multiple lambda -- therefore initializing it to NULL
        
    // -l
    if (my_input_parameters.fixed_lambda > 0.0) {
        cout << "Specified lambda (-l): " << my_input_parameters.fixed_lambda << endl;
        p_lambda = new single_lambda(&my_calculator, my_input_parameters.fixed_lambda);
            // vector<double> posterior = get_posterior((*p_gene_families), max_family_size, fixed_lambda, p_tree);
            // double map = log(*max_element(posterior.begin(), posterior.end()));
            // cout << "Posterior values found - max log posterior is " << map << endl;
			// call_viterbi(max_family_size, max_root_family_size, 15, p_lambda, *p_gene_families, p_tree);
        }
    
    // -m
    if (!my_input_parameters.fixed_multiple_lambdas.empty()) {       
        if (p_lambda_tree == NULL) {
            throw runtime_error("You must specify a lambda tree (-y) if you fix multiple lambda values (-m). Exiting..."); 
        }
        
        map<std::string, int> node_name_to_lambda_index = p_lambda_tree->get_lambda_index_map(); // allows matching lambda values (when -m) to clades in lambda tree

        vector<string> lambdastrings = tokenize_str(my_input_parameters.fixed_multiple_lambdas, ',');
	vector<double> lambdas(lambdastrings.size());

        // transform is like R's apply (lambdas takes the outputs)
        transform(lambdastrings.begin(), lambdastrings.end(), lambdas.begin(),
                [](string const& val) { return stod(val); } // this is the equivalent of a Python's lambda function
                );
        
        p_lambda = new multiple_lambda(&my_calculator, node_name_to_lambda_index, lambdas);
    }
    
    return p_lambda;
} 

void execute::infer(std::vector<core *>& models, std::vector<gene_family> *p_gene_families, clade *p_tree, lambda *p_lambda, const input_parameters &my_input_parameters, int max_family_size, int max_root_family_size)
{
    for (int i = 0; i < models.size(); ++i) {
        models[i]->set_gene_families(p_gene_families);
        models[i]->set_tree(p_tree);
        
        gamma_core* p_model = dynamic_cast<gamma_core *>(models[i]);
        if (p_model != NULL) {
            /* -k */
            p_model->adjust_n_gamma_cats(my_input_parameters.n_gamma_cats);
            p_model->adjust_family_gamma_membership(p_gene_families->size());
            double alpha = 0.5;
            p_model->set_alpha(alpha);
        }

        models[i]->set_max_sizes(max_family_size, max_root_family_size);
        models[i]->set_lambda(p_lambda);

        cout << endl << "Starting inference processes for model " << i << endl;
        models[i]->start_inference_processes();

        cout << endl << "Inferring processes for model " << i << endl;
        models[i]->infer_processes();
    }
}