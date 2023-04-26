#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include <functional>

#include <date/date.h>
#include <nlohmann/json.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <tbb/tbb.h>
#include "internship.h"
#include "OutputDataElement.h"

using json = nlohmann::json;
using namespace date;

namespace internship {

    const std::string OS_KEY = "os";
    const std::string PRODUCT_VERSIONS_KEY = "versions";
    const std::string NAME_KEY = "name";
    const std::string CYCLE_KEY = "cycle";
    const std::string RELEASE_DATE_KEY = "releaseDate";
    const std::string EOL_KEY = "eol";

    class AdvancedVector {
    private:
        std::vector<OutputDataElement> vec;
        unsigned int currentIndex = 0;
        bool end = false;
    public:
        void push_back(OutputDataElement element) {
            vec.push_back(element);
        }

        const OutputDataElement getCurrent() const {
            return vec[currentIndex];
        }

        void sort() {
            std::sort(vec.begin(), vec.end());
        }

        void moveCurrent() {
            currentIndex++;
            if (currentIndex >= vec.size()) {
                end = true;
            }
        }

        const unsigned int getCurrentIndex() const {
            return currentIndex;
        }

        const bool isEnd() const {
            if (vec.size() == 0) {
                return true;
            }

            return end;
        }

        std::vector<OutputDataElement> getVec() {
            return vec;
        }
    };

    const int THREADS_NUMBER = 4;
    boost::asio::thread_pool pool(THREADS_NUMBER);
    tbb::concurrent_vector<AdvancedVector> vectors;

    long countSupportPeriodInDays(std::string date1, std::string date2) {
        using namespace boost;

        try {
            gregorian::date d1(gregorian::from_simple_string(date1));
            gregorian::date d2(gregorian::from_simple_string(date2));
            gregorian::date_period dp(d1, d2);
            return abs(dp.length().days()) + 1;
        }
        catch (...) {
            throw std::runtime_error("Error parsing dates");
        }
    }

    void collectVersions(const std::vector<json>* products) {
        AdvancedVector batch;
        for (const json& product : *products) {
            for (const json& version : product[PRODUCT_VERSIONS_KEY]) {
                OutputDataElement element;
                try {
                    element.setOsName(product[NAME_KEY]);
                    element.setOsCycle(version[CYCLE_KEY]);
                    element.setSupportPeriodInDays(countSupportPeriodInDays(version[RELEASE_DATE_KEY], version[EOL_KEY]));
                    batch.push_back(element);
                }
                catch (const std::exception& exc) {
                    std::cerr << exc.what() << std::endl;
                    continue;
                }
            }
        }
        batch.sort();
        vectors.push_back(batch);
        delete products;
    }

    OutputDataElement getCurrent() {
        OutputDataElement max;
        AdvancedVector* maxVector = &vectors[0];
        max.setSupportPeriodInDays(0);
        for (AdvancedVector& vector : vectors) {
            if (!vector.isEnd()) {
                if (vector.getCurrent().getSupportPeriodInDays() > max.getSupportPeriodInDays()) {
                    max = vector.getCurrent();
                    maxVector = &vector;
                }
            }
        }
        maxVector->moveCurrent();

        return max;
    }

    std::vector<OutputDataElement> parse(const std::string& jsonFileName, int elementsCount) {
        std::vector<OutputDataElement> filteredProducts;
        
        if (elementsCount <= 0) {
            return filteredProducts;
        }

        std::ifstream f(jsonFileName);
        json data = json::parse(f);

        std::vector<json>* products = new std::vector<json>();
        int ctr = 0;
        for (const auto& [id, product] : data.items()) {
            if (product[OS_KEY] == true) {
                products->push_back(product);
                ctr++;
            }

            if (ctr >= 5) {
                auto task = std::bind(collectVersions, products);
                boost::asio::post(pool, task);
                ctr = 0;
                products = new std::vector<json>();
            }

        }
        collectVersions(products);
        
        pool.join();
        unsigned int counter = elementsCount;
        std::vector<OutputDataElement> solution;
        std::vector<AdvancedVector> resultVector;
        std::copy(vectors.begin(), vectors.end(), std::back_inserter(resultVector));

        while (counter > 0) {
            OutputDataElement max = getCurrent();
            solution.push_back(max);
            counter--;
        }

        return solution;
    }

    // do not remove this function
    void solution(const std::string& jsonFileName, int elementsCount) {
        std::vector<OutputDataElement> products = parse(jsonFileName, elementsCount);

        for (int i = 0; i < products.size(); i++) {
            OutputDataElement j = products[i];
            std::cout << j << std::endl;
        }
    }
}