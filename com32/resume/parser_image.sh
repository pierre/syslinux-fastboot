# Beginning of data, PAGE_SIZE aligned
page=25
j=$(($page * 4096))
let start_pfn=`od -j$j -N4 -Ad -t u4 image | awk '{print $2}'`
while [[ $start_pfn != 0 ]]; do
	let page=$page+1
	echo Go to page $page
	j=$(($page * 4096))
	let start_pfn=`od -j$j -N4 -Ad -t u4 image | awk '{print $2}'`
	echo $start_pfn
	if [[ $page -gt 35 ]]; then
		echo Unable to find start of the data
		exit 1
	fi
done
# Number of pfns found
pfns_found=0

# MAX_PFNS can be found in the header
MAX_PFNS=10000

while [[ $pfns_found -lt $MAX_PFNS ]]; do
	let tmp2=$j+4
	let size=`od -j$tmp2 -N4 -Ad -t u4 image | awk '{print $2}'`

	# Check the size of the page (possible compressed)
	if [[ (! $? -eq 0) || $size -gt 4096 ]]; then
		let j=$j+1
		continue
	else
		let pfns_found=$pfns_found+1
		# Output format: offset page_size sum_pfns_founds pfn_page
		# To check if the image is valid, check the last column (pfn numbers).
		# It should be roughly monotonically increasing, with realistic
		# frame numbers.
		echo "$j	[$size]	($pfns_found)		`od -j$j -N4 -Ad -tu4 image | awk '{print $2}'`"
		let j=$j+$size+8
	fi
done
